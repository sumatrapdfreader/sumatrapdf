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

#include "DjVuDocEditor.h"
#include "DjVuImage.h"
#include "IFFByteStream.h"
#include "DataPool.h"
#include "IW44Image.h"
#include "GOS.h"
#include "GURL.h"
#include "DjVuAnno.h"
#include "GRect.h"
#include "DjVmNav.h"

#include "debug.h"

#include <ctype.h>

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


static const char octets[4]={0x41,0x54,0x26,0x54};

int        DjVuDocEditor::thumbnails_per_file=10;

// This is a structure for active files and DataPools. It may contain
// a DjVuFile, which is currently being used by someone (I check the list
// and get rid of hanging files from time to time) or a DataPool,
// which is "custom" with respect to the document (was modified or
// inserted), or both.
//
// DjVuFile is set to smth!=0 when it's created using url_to_file().
//          It's reset back to ZERO in clean_files_map() when
//	  it sees, that a given file is not used by anyone.
// DataPool is updated when a file is inserted
class DjVuDocEditor::File : public GPEnabled
{
public:
  // 'pool' below may be non-zero only if it cannot be retrieved
  // by the DjVuDocument, that is it either corresponds to a
  // modified DjVuFile or it has been inserted. Otherwise it's ZERO
  // Once someone assigns a non-zero DataPool, it remains non-ZERO
  // (may be updated if the file gets modified) and may be reset
  // only by save() or save_as() functions.
  GP<DataPool>	pool;

  // If 'file' is non-zero, it means, that it's being used by someone
  // We check for unused files from time to time and ZERO them.
  // But before we do it, we may save the DataPool in the case if
  // file has been modified.
  GP<DjVuFile>	file;
};

void
DjVuDocEditor::check(void)
{
   if (!initialized) G_THROW( ERR_MSG("DjVuDocEditor.not_init") );
}

DjVuDocEditor::DjVuDocEditor(void)
{
   initialized=false;
   refresh_cb=0;
   refresh_cl_data=0;
}

DjVuDocEditor::~DjVuDocEditor(void)
{
   GCriticalSectionLock lock(&thumb_lock);
   thumb_map.empty();
   DataPool::close_all();
}

void
DjVuDocEditor::init(void)
{
   DEBUG_MSG("DjVuDocEditor::init() called\n");
   DEBUG_MAKE_INDENT(3);

      // If you remove this check be sure to delete thumb_map
   if (initialized) G_THROW( ERR_MSG("DjVuDocEditor.init") );

   doc_url=GURL::Filename::UTF8("noname.djvu");

   const GP<DjVmDoc> doc(DjVmDoc::create());
   const GP<ByteStream> gstr(ByteStream::create());
   doc->write(gstr);
   gstr->seek(0, SEEK_SET);
   doc_pool=DataPool::create(gstr);

   orig_doc_type=UNKNOWN_TYPE;
   orig_doc_pages=0;

   initialized=true;

   DjVuDocument::init(doc_url, this);
}

void
DjVuDocEditor::init(const GURL &url)
{
   DEBUG_MSG("DjVuDocEditor::init() called: url='" << url << "'\n");
   DEBUG_MAKE_INDENT(3);

      // If you remove this check be sure to delete thumb_map
   if (initialized)
     G_THROW( ERR_MSG("DjVuDocEditor.init") );

      // First - create a temporary DjVuDocument and check its type
   doc_pool=DataPool::create(url);
   doc_url=url;
   const GP<DjVuDocument> tmp_doc(DjVuDocument::create_wait(doc_url,this));
   if (!tmp_doc->is_init_ok())
      G_THROW( ERR_MSG("DjVuDocEditor.open_fail") "\t" +url.get_string());

   orig_doc_type=tmp_doc->get_doc_type();
   orig_doc_pages=tmp_doc->get_pages_num();
   if (orig_doc_type==OLD_BUNDLED ||
       orig_doc_type==OLD_INDEXED ||
       orig_doc_type==SINGLE_PAGE)
   {
     // Suxx. I need to convert it now.
     GP<ByteStream> gstr = ByteStream::create();  // Convert in memory.
     tmp_doc->write(gstr, true);  // Force DJVM format
     gstr->seek(0);                     
     doc_pool=DataPool::create(gstr);
   }

      // OK. Now doc_pool contains data of the document in one of the
      // new formats. It will be a lot easier to insert/delete pages now.
      // 'doc_url' below of course doesn't refer to the file with the converted
      // data, but we will take care of it by redirecting the request_data().
   initialized=true;
   DjVuDocument::init(doc_url, this);

      // Cool. Now extract the thumbnails...
   GCriticalSectionLock lock(&thumb_lock);
   int pages_num=get_pages_num();
   for(int page_num=0;page_num<pages_num;page_num++)
   {
	 // Call DjVuDocument::get_thumbnail() here to bypass logic
	 // of DjVuDocEditor::get_thumbnail(). init() is the only safe
	 // place where we can still call DjVuDocument::get_thumbnail();
      const GP<DataPool> pool(DjVuDocument::get_thumbnail(page_num, true));
      if (pool)
      {
        thumb_map[page_to_id(page_num)]=pool;
      }
   }
      // And remove then from DjVmDir so that DjVuDocument
      // does not try to use them
   unfile_thumbnails();
}

GP<DataPool>
DjVuDocEditor::request_data(const DjVuPort * source, const GURL & url)
{
   DEBUG_MSG("DjVuDocEditor::request_data(): url='" << url << "'\n");
   DEBUG_MAKE_INDENT(3);

      // Check if we have either original data or converted (to new format),
      // if all the story is about the DjVuDocument's data
   if (url==doc_url)
     return doc_pool;

      // Now see if we have any file matching the url
   const GP<DjVmDir::File> frec(djvm_dir->name_to_file(url.fname()));
   if (frec)
   {
      GCriticalSectionLock lock(&files_lock);
      GPosition pos;
      if (files_map.contains(frec->get_load_name(), pos))
      {
         const GP<File> f(files_map[pos]);
         if (f->file && f->file->get_init_data_pool())
            return f->file->get_init_data_pool();// Favor DjVuFile's knowledge
         else if (f->pool) return f->pool;
      }
   }

      // Finally let DjVuDocument cope with it. It may be a connected DataPool
      // for a BUNDLED format. Or it may be a file. Anyway, it was not
      // manually included, so it should be in the document.
   const GP<DataPool> pool(DjVuDocument::request_data(source, url));

      // We do NOT update the 'File' structure, because our rule is that
      // we keep a separate copy of DataPool in 'File' only if it cannot
      // be retrieved from DjVuDocument (like it has been "inserted" or
      // corresponds to a modified file).
   return pool;
}

void
DjVuDocEditor::clean_files_map(void)
      // Will go thru the map of files looking for unreferenced
      // files or records w/o DjVuFile and DataPool.
      // These will be modified and/or removed.
{
   DEBUG_MSG("DjVuDocEditor::clean_files_map() called\n");
   DEBUG_MAKE_INDENT(3);

   GCriticalSectionLock lock(&files_lock);

      // See if there are too old items in the "cache", which are
      // not referenced by anyone. If the corresponding DjVuFile has been
      // modified, obtain the new data and replace the 'pool'. Clear the
      // DjVuFile anyway. If both DataPool and DjVuFile are zero, remove
      // the entry.
   for(GPosition pos=files_map;pos;)
   {
      const GP<File> f(files_map[pos]);
      if (f->file && f->file->get_count()==1)
      {
         DEBUG_MSG("ZEROing file '" << f->file->get_url() << "'\n");
         if (f->file->is_modified())
            f->pool=f->file->get_djvu_data(false);
         f->file=0;
      }
      if (!f->file && !f->pool)
      {
         DEBUG_MSG("Removing record '" << files_map.key(pos) << "'\n");
         GPosition this_pos=pos;
         ++pos;
         files_map.del(this_pos);
      } else ++pos;
   }
}

GP<DjVuFile>
DjVuDocEditor::url_to_file(const GURL & url, bool dont_create) const
{
   DEBUG_MSG("DjVuDocEditor::url_to_file(): url='" << url << "'\n");
   DEBUG_MAKE_INDENT(3);

      // Check if have a DjVuFile with this url cached (created before
      // and either still active or left because it has been modified)
   GP<DjVmDir::File> frec;
   if((const DjVmDir *)djvm_dir)
     frec=djvm_dir->name_to_file(url.fname());
   if (frec)
   {
      GCriticalSectionLock lock(&(const_cast<DjVuDocEditor *>(this)->files_lock));
      GPosition pos;
      if (files_map.contains(frec->get_load_name(), pos))
      {
         const GP<File> f(files_map[pos]);
         if (f->file)
           return f->file;
      }
   }

   const_cast<DjVuDocEditor *>(this)->clean_files_map();

      // We don't have the file cached. Let DjVuDocument create the file.
   const GP<DjVuFile> file(DjVuDocument::url_to_file(url, dont_create));

      // And add it to our private "cache"
   if (file && frec)
   {
      GCriticalSectionLock lock(&(const_cast<DjVuDocEditor *>(this)->files_lock));
      GPosition pos;
      if (files_map.contains(frec->get_load_name(), pos))
      {
         files_map[frec->get_load_name()]->file=file;
      }else
      {
         const GP<File> f(new File());
         f->file=file;
         const_cast<DjVuDocEditor *>(this)->files_map[frec->get_load_name()]=f;
      }
   }

   return file;
}

GUTF8String
DjVuDocEditor::page_to_id(int page_num) const
{
   if (page_num<0 || page_num>=get_pages_num())
     G_THROW( ERR_MSG("DjVuDocEditor.page_num") "\t"+GUTF8String(page_num));
   const GP<DjVmDir::File> f(djvm_dir->page_to_file(page_num));
   if (! f)
     G_THROW( ERR_MSG("DjVuDocEditor.page_num") "\t"+GUTF8String(page_num));

   return f->get_load_name();
}

GUTF8String
DjVuDocEditor::find_unique_id(GUTF8String id)
{
  const GP<DjVmDir> dir(get_djvm_dir());

  GUTF8String base, ext;
  const int dot=id.rsearch('.');
  if(dot >= 0)
  {
    base=id.substr(0,dot);
    ext=id.substr(dot+1,(unsigned int)-1);
  }else
  {
    base=id;
  }

  int cnt=0;
  while (!(!dir->id_to_file(id) &&
           !dir->name_to_file(id) &&
           !dir->title_to_file(id)))
  {
     cnt++;
     id=base+"_"+GUTF8String(cnt);
     if (ext.length())
       id+="."+ext;
  }
  return id;
}

GP<DataPool>
DjVuDocEditor::strip_incl_chunks(const GP<DataPool> & pool_in)
{
   DEBUG_MSG("DjVuDocEditor::strip_incl_chunks() called\n");
   DEBUG_MAKE_INDENT(3);

   const GP<IFFByteStream> giff_in(
     IFFByteStream::create(pool_in->get_stream()));

   const GP<ByteStream> gbs_out(ByteStream::create());
   const GP<IFFByteStream> giff_out(IFFByteStream::create(gbs_out));

   IFFByteStream &iff_in=*giff_in;
   IFFByteStream &iff_out=*giff_out;

   bool have_incl=false;
   int chksize;
   GUTF8String chkid;
   if (iff_in.get_chunk(chkid))
   {
      iff_out.put_chunk(chkid);
      while((chksize=iff_in.get_chunk(chkid)))
      {
         if (chkid!="INCL")
         {
            iff_out.put_chunk(chkid);
            iff_out.copy(*iff_in.get_bytestream());
            iff_out.close_chunk();
         } else
         {
           have_incl=true;
         }
         iff_in.close_chunk();
      }
      iff_out.close_chunk();
   }

   if (have_incl)
   {
      gbs_out->seek(0,SEEK_SET);
      return DataPool::create(gbs_out);
   } else return pool_in;
}

GUTF8String
DjVuDocEditor::insert_file(const GURL &file_url, const GUTF8String &parent_id,
                           int chunk_num, DjVuPort *source)
      // Will open the 'file_name' and insert it into an existing DjVuFile
      // with ID 'parent_id'. Will insert the INCL chunk at position chunk_num
      // Will NOT process ANY files included into the file being inserted.
      // Moreover it will strip out any INCL chunks in that file...
{
   DEBUG_MSG("DjVuDocEditor::insert_file(): fname='" << file_url <<
             "', parent_id='" << parent_id << "'\n");
   DEBUG_MAKE_INDENT(3);
   const GP<DjVmDir> dir(get_djvm_dir());

   if(!source)
     source=this;
      // Create DataPool and see if the file exists
   GP<DataPool> file_pool;
   if(file_url.is_empty()||file_url.is_local_file_url())
   {
     file_pool=DataPool::create(file_url);
   }else
   {
     file_pool=source->request_data(source, file_url);
     if(source != this)
     {
       file_pool=DataPool::create(file_pool->get_stream());
     }
   }
   if(file_pool && file_url && DjVuDocument::djvu_import_codec)
   {
     (*DjVuDocument::djvu_import_codec)(file_pool,file_url,needs_compression_flag,can_compress_flag);
   }

      // Strip any INCL chunks
   file_pool=strip_incl_chunks(file_pool);

      // Check if parent ID is valid
   GP<DjVmDir::File> parent_frec(dir->id_to_file(parent_id));
   if (!parent_frec)
     parent_frec=dir->name_to_file(parent_id);
   if (!parent_frec)
     parent_frec=dir->title_to_file(parent_id);
   if (!parent_frec)
     G_THROW( ERR_MSG("DjVuDocEditor.no_file") "\t" +parent_id);
   const GP<DjVuFile> parent_file(get_djvu_file(parent_id));
   if (!parent_file)
     G_THROW( ERR_MSG("DjVuDocEditor.create_fail") "\t"+parent_id);

      // Now obtain ID for the new file
   const GUTF8String id(find_unique_id(file_url.fname()));

      // Add it into the directory
   const GP<DjVmDir::File> frec(
     DjVmDir::File::create(id, id, id, DjVmDir::File::INCLUDE));
   int pos=dir->get_file_pos(parent_frec);
   if (pos>=0)
     ++pos;
   dir->insert_file(frec, pos);

      // Add it to our "cache"
   {
      const GP<File> f(new File);
      f->pool=file_pool;
      GCriticalSectionLock lock(&files_lock);
      files_map[id]=f;
   }

      // And insert it into the parent DjVuFile
   parent_file->insert_file(id, chunk_num);

   return id;
}

      // First it will insert the 'file_url' at position 'file_pos'.
      //
      // Then it will process all the INCL chunks in the file and try to do
      // the same thing with the included files. If insertion of an included
      // file fails, it will proceed with other INCL chunks until it does
      // them all. In the very end we will throw exception to let the caller
      // know about problems with included files.
      //
      // If the name of a file being inserted conflicts with some other
      // name, which has been in DjVmDir prior to call to this function,
      // it will be modified. name2id is the translation table to
      // keep track of these modifications.
      //
      // Also, if a name is in name2id, we will not insert that file again.
      //
      // Will return TRUE if the file has been successfully inserted.
      // FALSE, if the file contains NDIR chunk and has been skipped.
bool
DjVuDocEditor::insert_file(const GURL &file_url, bool is_page,
  int & file_pos, GMap<GUTF8String, GUTF8String> & name2id,
  DjVuPort *source)
{

  DEBUG_MSG("DjVuDocEditor::insert_file(): file_url='" << file_url <<
             "', is_page='" << is_page << "'\n");
  DEBUG_MAKE_INDENT(3);
  if (refresh_cb)
    refresh_cb(refresh_cl_data);


      // We do not want to insert the same file twice (important when
      // we insert a group of files at the same time using insert_group())
      // So we check if we already did that and return if so.
  if (name2id.contains(file_url.fname()))
    return true;

  if(!source)
    source=this;

  GP<DataPool> file_pool;
  if(file_url.is_empty()||file_url.is_local_file_url())
  {
    file_pool=DataPool::create(file_url);
  }
  else
  {
    file_pool=source->request_data(source, file_url);
    if(source != this)
    {
      file_pool=DataPool::create(file_pool->get_stream());
    }
  }
       // Create DataPool and see if the file exists
  if(file_pool && !file_url.is_empty() && DjVuDocument::djvu_import_codec)
  {
      (*DjVuDocument::djvu_import_codec)(file_pool,file_url,
                                         needs_compression_flag,
                                         can_compress_flag);
  }

  // Oh. It does exist... Check that it has IFF structure
  {
    const GP<IFFByteStream> giff(
       IFFByteStream::create(file_pool->get_stream()));
    IFFByteStream &iff=*giff;
    GUTF8String chkid;
    iff.get_chunk(chkid);
    if (chkid!="FORM:DJVI" && chkid!="FORM:DJVU" &&
        chkid!="FORM:BM44" && chkid!="FORM:PM44")
      G_THROW( ERR_MSG("DjVuDocEditor.not_1_page") "\t"
               + file_url.get_string());
    // Wonderful. It's even a DjVu file. Scan for NDIR chunks.
    // If NDIR chunk is found, ignore the file
    while(iff.get_chunk(chkid))
      {
        if (chkid=="NDIR")
          return false;
        iff.close_chunk();
      }
  }
  return insert_file(file_pool,file_url,is_page,file_pos,name2id,source);
}

bool
DjVuDocEditor::insert_file(const GP<DataPool> &file_pool,
  const GURL &file_url, bool is_page,
  int & file_pos, GMap<GUTF8String, GUTF8String> & name2id,
  DjVuPort *source)
{
  GUTF8String errors;
  if(file_pool)
  {
    const GP<DjVmDir> dir(get_djvm_dir());
    G_TRY
    {
         // Now get a unique name for this file.
         // Check the name2id first...
      const GUTF8String name=file_url.fname();
      GUTF8String id;
      if (name2id.contains(name))
      {
        id=name2id[name];
      }else
      {
           // Check to see if this page exists with a different name.
        if(!is_page)
        {
          GPList<DjVmDir::File> list(dir->get_files_list());
          for(GPosition pos=list;pos;++pos)
          {
            DEBUG_MSG("include " << list[pos]->is_include() 
                      << " size=" << list[pos]->size << " length=" 
                      << file_pool->get_length() << "\n");
            if(list[pos]->is_include() 
               && (!list[pos]->size 
                   || (list[pos]->size == file_pool->get_length())))
            {
              id=list[pos]->get_load_name();
              GP<DjVuFile> file(get_djvu_file(id,false));
              const GP<DataPool> pool(file->get_djvu_data(false));
              if(file_pool->simple_compare(*pool))
              {
                // The files are the same, so just store the alias.
                name2id[name]=id;
              }
              const GP<IFFByteStream> giff_old(IFFByteStream::create(pool->get_stream()));
              const GP<IFFByteStream> giff_new(IFFByteStream::create(file_pool->get_stream()));
              file=0;
              if(giff_old->compare(*giff_new))
              {
                // The files are the same, so just store the alias.
                name2id[name]=id;
                return true;
              }
            } 
          }
        }
        // Otherwise create a new unique ID and remember the translation
        id=find_unique_id(name);
        name2id[name]=id;
      }

         // Good. Before we continue with the included files we want to
         // complete insertion of this one. Notice, that insertion of
         // children may fail, in which case we will have to modify
         // data for this file to get rid of invalid INCL

         // Create a file record with the chosen ID
      const GP<DjVmDir::File> file(DjVmDir::File::create(id, id, id,
        is_page ? DjVmDir::File::PAGE : DjVmDir::File::INCLUDE ));

         // And insert it into the directory
      file_pos=dir->insert_file(file, file_pos);

         // And add the File record (containing the file URL and DataPool)
      {
         const GP<File> f(new File);
         f->pool=file_pool;
         GCriticalSectionLock lock(&files_lock);
         files_map[id]=f;
      }

         // The file has been added. If it doesn't include anything else,
         // that will be enough. Otherwise repeat what we just did for every
         // included child. Don't forget to modify the contents of INCL
         // chunks due to name2id translation.
         // We also want to include here our file with shared annotations,
         // if it exists.
      GUTF8String chkid;
      const GP<IFFByteStream> giff_in(
        IFFByteStream::create(file_pool->get_stream()));
      IFFByteStream &iff_in=*giff_in;
      const GP<ByteStream> gstr_out(ByteStream::create());
      const GP<IFFByteStream> giff_out(IFFByteStream::create(gstr_out));
      IFFByteStream &iff_out=*giff_out;

      const GP<DjVmDir::File> shared_frec(djvm_dir->get_shared_anno_file());

      iff_in.get_chunk(chkid);
      iff_out.put_chunk(chkid);
      while(iff_in.get_chunk(chkid))
      {
         if (chkid!="INCL")
         {
            iff_out.put_chunk(chkid);
            iff_out.copy(*iff_in.get_bytestream());
            iff_in.close_chunk();
            iff_out.close_chunk();
            if (shared_frec && chkid=="INFO")
            {
               iff_out.put_chunk("INCL");
               iff_out.get_bytestream()->writestring(shared_frec->get_load_name());
               iff_out.close_chunk();
            }
         } else
         {
            GUTF8String name;
            char buffer[1024];
            int length;
            while((length=iff_in.read(buffer, 1024)))
               name+=GUTF8String(buffer, length);
            while(isspace((unsigned char)name[0]))
            {
              name=name.substr(1,(unsigned int)-1);
            }
            while(isspace((unsigned char)name[(int)name.length()-1]))
            {
              name.setat(name.length()-1, 0);
            }
            const GURL::UTF8 full_url(name,file_url.base());
            iff_in.close_chunk();

            G_TRY {
               if (insert_file(full_url, false, file_pos, name2id, source))
               {
                     // If the child file has been inserted (doesn't
                     // contain NDIR chunk), add INCL chunk.
                  GUTF8String id=name2id[name];
                  iff_out.put_chunk("INCL");
                  iff_out.get_bytestream()->writestring(id);
                  iff_out.close_chunk();
               }
            } G_CATCH(exc) {
                  // Should an error occur, we move on. INCL chunk will
                  // not be copied.
               if (errors.length())
                 errors+="\n\n";
               errors+=exc.get_cause();
            } G_ENDCATCH;
         }
      } // while(iff_in.get_chunk(chkid))
      iff_out.close_chunk();

      // Increment the file_pos past the page inserted.
      if (file_pos>=0) file_pos++;

         // We have just inserted every included file. We may have modified
         // contents of the INCL chunks. So we need to update the DataPool...
      gstr_out->seek(0);
      const GP<DataPool> new_file_pool(DataPool::create(gstr_out));
      {
            // It's important that we replace the pool here anyway.
            // By doing this we load the file into memory. And this is
            // exactly what insert_group() wants us to do because
            // it creates temporary files.
         GCriticalSectionLock lock(&files_lock);
         files_map[id]->pool=new_file_pool;
      }
    } G_CATCH(exc) {
      if (errors.length())
        errors+="\n\n";
      errors+=exc.get_cause();
      G_THROW(errors);
    } G_ENDCATCH;

      // The only place where we intercept exceptions is when we process
      // included files. We want to process all of them even if we failed to
      // process one. But here we need to let the exception propagate...
    if (errors.length())
      G_THROW(errors);

    return true;
  }
  return false;
}

void
DjVuDocEditor::insert_group(const GList<GURL> & file_urls, int page_num,
                             void (* _refresh_cb)(void *), void * _cl_data)
      // The function will insert every file from the list at position
      // corresponding to page_num. If page_num is negative, concatenation
      // will occur. Included files will be processed as well
{
  refresh_cb=_refresh_cb;
  refresh_cl_data=_cl_data;

  G_TRY
  {

     // First translate the page_num to file_pos.
    const GP<DjVmDir> dir(get_djvm_dir());
    int file_pos;
    if (page_num<0 || page_num>=dir->get_pages_num())
    {
      file_pos=-1;
    }
    else
    {
      file_pos=dir->get_page_pos(page_num);
    }

       // Now call the insert_file() for every page. We will remember the
       // name2id translation table. Thus insert_file() will remember IDs
       // it assigned to shared files
    GMap<GUTF8String, GUTF8String> name2id;

    GUTF8String errors;
    for(GPosition pos=file_urls;pos;++pos)
    {
      const GURL &furl=file_urls[pos];
      DEBUG_MSG( "Inserting file '" << furl << "'\n" );
      G_TRY
      {
               // Check if it's a multipage document...
        GP<DataPool> xdata_pool(DataPool::create(furl));
        if(xdata_pool && furl.is_valid()
           && furl.is_local_file_url() && DjVuDocument::djvu_import_codec)
        {
          (*DjVuDocument::djvu_import_codec)(xdata_pool,furl,
                                             needs_compression_flag,
                                             can_compress_flag);
        }
        GUTF8String chkid;
        IFFByteStream::create(xdata_pool->get_stream())->get_chunk(chkid);
        if (name2id.contains(furl.fname())||(chkid=="FORM:DJVM"))
        {
          GMap<GUTF8String,void *> map;
          map_ids(map);
          DEBUG_MSG("Read DjVuDocument furl='" << furl << "'\n");
          GP<ByteStream> gbs(ByteStream::create());
          GP<DjVuDocument> doca(DjVuDocument::create_noinit());
          doca->set_verbose_eof(verbose_eof);
          doca->set_recover_errors(recover_errors);
          doca->init(furl /* ,this */ );
          doca->wait_for_complete_init();
          get_portcaster()->add_route(doca,this);
          DEBUG_MSG("Saving DjVuDocument url='" << furl << "' with unique names\n");
          doca->write(gbs,map);
          gbs->seek(0L);
          DEBUG_MSG("Loading unique names\n");
          GP<DjVuDocument> doc(DjVuDocument::create(gbs));
          doc->set_verbose_eof(verbose_eof);
          doc->set_recover_errors(recover_errors);
          doc->wait_for_complete_init();
          get_portcaster()->add_route(doc,this);
          gbs=0;
          DEBUG_MSG("Inserting pages\n");
          int pages_num=doc->get_pages_num();
          for(int page_num=0;page_num<pages_num;page_num++)
          {
            const GURL url(doc->page_to_url(page_num));
            insert_file(url, true, file_pos, name2id, doc);
          }
        }
        else
        {
          insert_file(furl, true, file_pos, name2id, this);
        }
      } G_CATCH(exc)
      {
        if (errors.length())
        {
          errors+="\n\n";
        }
        errors+=exc.get_cause();
      }
      G_ENDCATCH;
    }
    if (errors.length())
    {
      G_THROW(errors);
    }
  } G_CATCH_ALL
  {
    refresh_cb=0;
    refresh_cl_data=0;
    G_RETHROW;
  } G_ENDCATCH;
  refresh_cb=0;
  refresh_cl_data=0;
}

void
DjVuDocEditor::insert_page(const GURL &file_url, int page_num)
{
   DEBUG_MSG("DjVuDocEditor::insert_page(): furl='" << file_url << "'\n");
   DEBUG_MAKE_INDENT(3);

   GList<GURL> list;
   list.append(file_url);

   insert_group(list, page_num);
}

void
DjVuDocEditor::insert_page(GP<DataPool> & _file_pool,
			   const GURL & file_url, int page_num)
      // Use _file_pool as source of data, create a new DjVuFile
      // with name file_name, and insert it as page number page_num
{
   DEBUG_MSG("DjVuDocEditor::insert_page(): pool size='" <<
	     _file_pool->get_size() << "'\n");
   DEBUG_MAKE_INDENT(3);

   const GP<DjVmDir> dir(get_djvm_dir());

      // Strip any INCL chunks (we do not allow to insert hierarchies
      // using this function)
   const GP<DataPool> file_pool(strip_incl_chunks(_file_pool));
   
      // Now obtain ID for the new file
   const GUTF8String id(find_unique_id(file_url.fname()));

      // Add it into the directory
   const GP<DjVmDir::File> frec(DjVmDir::File::create(
     id, id, id, DjVmDir::File::PAGE));
   int pos=dir->get_page_pos(page_num);
   dir->insert_file(frec, pos);

      // Add it to our "cache"
   {
      GP<File> f=new File;
      f->pool=file_pool;
      GCriticalSectionLock lock(&files_lock);
      files_map[id]=f;
   }
}

void
DjVuDocEditor::generate_ref_map(const GP<DjVuFile> & file,
				GMap<GUTF8String, void *> & ref_map,
				GMap<GURL, void *> & visit_map)
      // This private function is used to generate a list (implemented as map)
      // of files referencing the given file. To get list of all parents
      // for file with ID 'id' iterate map obtained as
      // *((GMap<GUTF8String, void *> *) ref_map[id])
{
   const GURL url=file->get_url();
   const GUTF8String id(djvm_dir->name_to_file(url.fname())->get_load_name());
   if (!visit_map.contains(url))
   {
      visit_map[url]=0;

      GPList<DjVuFile> files_list=file->get_included_files(false);
      for(GPosition pos=files_list;pos;++pos)
      {
         GP<DjVuFile> child_file=files_list[pos];
            // First: add the current file to the list of parents for
            // the child being processed
         GURL child_url=child_file->get_url();
         const GUTF8String child_id(
           djvm_dir->name_to_file(child_url.fname())->get_load_name());
         GMap<GUTF8String, void *> * parents=0;
         if (ref_map.contains(child_id))
            parents=(GMap<GUTF8String, void *> *) ref_map[child_id];
         else
            ref_map[child_id]=parents=new GMap<GUTF8String, void *>();
         (*parents)[id]=0;
            // Second: go recursively
         generate_ref_map(child_file, ref_map, visit_map);
      }
   }
}

void
DjVuDocEditor::remove_file(const GUTF8String &id, bool remove_unref,
                           GMap<GUTF8String, void *> & ref_map)
      // Private function, which will remove file with ID id.
      //
      // If will also remove all INCL chunks in parent files pointing
      // to this one
      //
      // Finally, if remove_unref is TRUE, we will go down the files
      // hierarchy removing every file, which becomes unreferenced.
      //
      // ref_map will be used to find out list of parents referencing
      // this file (required when removing INCL chunks)
{
      // First get rid of INCL chunks in parents
   GMap<GUTF8String, void *> * parents=(GMap<GUTF8String, void *> *) ref_map[id];
   if (parents)
   {
      for(GPosition pos=*parents;pos;++pos)
      {
         const GUTF8String parent_id((*parents).key(pos));
         const GP<DjVuFile> parent(get_djvu_file(parent_id));
         if (parent)
           parent->unlink_file(id);
      }
      delete parents;
      parents=0;
      ref_map.del(id);
   }

      // We will accumulate errors here.
   GUTF8String errors;

      // Now modify the ref_map and process children if necessary
   GP<DjVuFile> file=get_djvu_file(id);
   if (file)
   {
      G_TRY {
         GPList<DjVuFile> files_list=file->get_included_files(false);
         for(GPosition pos=files_list;pos;++pos)
         {
            GP<DjVuFile> child_file=files_list[pos];
            GURL child_url=child_file->get_url();
            const GUTF8String child_id(
              djvm_dir->name_to_file(child_url.fname())->get_load_name());
            GMap<GUTF8String, void *> * parents=(GMap<GUTF8String, void *> *) ref_map[child_id];
            if (parents) parents->del(id);

            if (remove_unref && (!parents || !parents->size()))
               remove_file(child_id, remove_unref, ref_map);
         }
      } G_CATCH(exc) {
         if (errors.length()) errors+="\n\n";
         errors+=exc.get_cause();
      } G_ENDCATCH;
   }

      // Finally remove this file from the directory.
   djvm_dir->delete_file(id);

      // And get rid of its thumbnail, if any
   GCriticalSectionLock lock(&thumb_lock);
   GPosition pos(thumb_map.contains(id));
   if (pos)
   {
     thumb_map.del(pos);
   }
   if (errors.length())
     G_THROW(errors);
}

void
DjVuDocEditor::remove_file(const GUTF8String &id, bool remove_unref)
{
   DEBUG_MSG("DjVuDocEditor::remove_file(): id='" << id << "'\n");
   DEBUG_MAKE_INDENT(3);

   if (!djvm_dir->id_to_file(id))
      G_THROW( ERR_MSG("DjVuDocEditor.no_file") "\t"+id);

      // First generate a map of references (containing the list of parents
      // including this particular file. This will speed things up
      // significatly.
   GMap<GUTF8String, void *> ref_map;        // GMap<GUTF8String, GMap<GUTF8String, void *> *> in fact
   GMap<GURL, void *> visit_map;        // To avoid loops

   int pages_num=djvm_dir->get_pages_num();
   for(int page_num=0;page_num<pages_num;page_num++)
      generate_ref_map(get_djvu_file(page_num), ref_map, visit_map);

      // Now call the function, which will do the removal recursively
   remove_file(id, remove_unref, ref_map);

      // And clear the ref_map
   GPosition pos;
   while((pos=ref_map))
   {
      GMap<GUTF8String, void *> * parents=(GMap<GUTF8String, void *> *) ref_map[pos];
      delete parents;
      ref_map.del(pos);
   }
}

void
DjVuDocEditor::remove_page(int page_num, bool remove_unref)
{
   DEBUG_MSG("DjVuDocEditor::remove_page(): page_num=" << page_num << "\n");
   DEBUG_MAKE_INDENT(3);

      // Translate the page_num to ID
   GP<DjVmDir> djvm_dir=get_djvm_dir();
   if (page_num<0 || page_num>=djvm_dir->get_pages_num())
      G_THROW( ERR_MSG("DjVuDocEditor.bad_page") "\t"+GUTF8String(page_num));

      // And call general remove_file()
   remove_file(djvm_dir->page_to_file(page_num)->get_load_name(), remove_unref);
}

void
DjVuDocEditor::remove_pages(const GList<int> & page_list, bool remove_unref)
{
   DEBUG_MSG("DjVuDocEditor::remove_pages() called\n");
   DEBUG_MAKE_INDENT(3);

      // First we need to translate page numbers to IDs (they will
      // obviously be changing while we're removing pages one after another)
   GP<DjVmDir> djvm_dir=get_djvm_dir();
   GPosition pos ;
   if (djvm_dir)
   {
      GList<GUTF8String> id_list;
      for(pos=page_list;pos;++pos)
      {
         GP<DjVmDir::File> frec=djvm_dir->page_to_file(page_list[pos]);
         if (frec)
            id_list.append(frec->get_load_name());
      }

      for(pos=id_list;pos;++pos)
      {
         GP<DjVmDir::File> frec=djvm_dir->id_to_file(id_list[pos]);
         if (frec)
            remove_page(frec->get_page_num(), remove_unref);
      }
   }
}

void
DjVuDocEditor::move_file(const GUTF8String &id, int & file_pos,
                         GMap<GUTF8String, void *> & map)
      // NOTE! file_pos here is the desired position in DjVmDir *after*
      // the record with ID 'id' is removed.
{
   if (!map.contains(id))
   {
      map[id]=0;

      GP<DjVmDir::File> file_rec=djvm_dir->id_to_file(id);
      if (file_rec)
      {
         file_rec=new DjVmDir::File(*file_rec);
         djvm_dir->delete_file(id);
         djvm_dir->insert_file(file_rec, file_pos);

         if (file_pos>=0)
         {
            file_pos++;
        
               // We care to move included files only if we do not append
               // This is because the only reason why we move included
               // files is to made them available sooner than they would
               // be available if we didn't move them. By appending files
               // we delay the moment when the data for the file becomes
               // available, of course.
            GP<DjVuFile> djvu_file=get_djvu_file(id);
            if (djvu_file)
            {
               GPList<DjVuFile> files_list=djvu_file->get_included_files(false);
               for(GPosition pos=files_list;pos;++pos)
               {
                  const GUTF8String name(files_list[pos]->get_url().fname());
                  GP<DjVmDir::File> child_frec=djvm_dir->name_to_file(name);

                     // If the child is positioned in DjVmDir AFTER the
                     // file being processed (position is file_pos or greater),
                     // move it to file_pos position
                  if (child_frec)
                     if (djvm_dir->get_file_pos(child_frec)>file_pos)
                        move_file(child_frec->get_load_name(), file_pos, map);
               }
            }
         }
      }
   }
}

void
DjVuDocEditor::move_page(int page_num, int new_page_num)
{
  DEBUG_MSG("DjVuDocEditor::move_page(): page_num=" << page_num <<
	    ", new_page_num=" << new_page_num << "\n");
  DEBUG_MAKE_INDENT(3);

  if (page_num==new_page_num) return;

  int pages_num=get_pages_num();
  if (page_num<0 || page_num>=pages_num)
    G_THROW( ERR_MSG("DjVuDocEditor.bad_page") "\t"+GUTF8String(page_num));

  const GUTF8String id(page_to_id(page_num));
  int file_pos=-1;
  if (new_page_num>=0 && new_page_num<pages_num)
    {
      if (new_page_num>page_num)        // Moving toward the end
	{
	  if (new_page_num<pages_num-1)
	    file_pos=djvm_dir->get_page_pos(new_page_num+1)-1;
	}
      else
	file_pos=djvm_dir->get_page_pos(new_page_num);
    }

  GMap<GUTF8String, void *> map;
  move_file(id, file_pos, map);
}

#ifdef _WIN32_WCE_EMULATION         // Work around odd behavior under WCE Emulation
#define CALLINGCONVENTION __cdecl
#else
#define CALLINGCONVENTION  /* */
#endif

static int
CALLINGCONVENTION
cmp(const void * ptr1, const void * ptr2)
{
   int num1=*(int *) ptr1;
   int num2=*(int *) ptr2;
   return num1<num2 ? -1 : num1>num2 ? 1 : 0;
}

static GList<int>
sortList(const GList<int> & list)
{
   GArray<int> a(list.size()-1);
   int cnt;
   GPosition pos;
   for(pos=list, cnt=0;pos;++pos, cnt++)
      a[cnt]=list[pos];

   qsort((int *) a, a.size(), sizeof(int), cmp);

   GList<int> l;
   for(int i=0;i<a.size();i++)
      l.append(a[i]);

   return l;
}

void
DjVuDocEditor::move_pages(const GList<int> & _page_list, int shift)
{
   if (!shift) return;

   GList<int> page_list=sortList(_page_list);

   GList<GUTF8String> id_list;
   for(GPosition pos=page_list;pos;++pos)
   {
      GP<DjVmDir::File> frec=djvm_dir->page_to_file(page_list[pos]);
      if (frec)
         id_list.append(frec->get_load_name());
   }

   if (shift<0)
   {
         // We have to start here from the smallest page number
         // We will move it according to the 'shift', and all
         // further moves are guaranteed not to affect its page number.

         // We will be changing the 'min_page' to make sure that
         // pages moved beyond the document will still be in correct order
      int min_page=0;
      for(GPosition pos=id_list;pos;++pos)
      {
         GP<DjVmDir::File> frec=djvm_dir->id_to_file(id_list[pos]);
         if (frec)
         {
            int page_num=frec->get_page_num();
            int new_page_num=page_num+shift;
            if (new_page_num<min_page)
               new_page_num=min_page++;
            move_page(page_num, new_page_num);
         }
      }
   } else
   {
         // We have to start here from the biggest page number
         // We will move it according to the 'shift', and all
         // further moves will not affect its page number.

         // We will be changing the 'max_page' to make sure that
         // pages moved beyond the document will still be in correct order
      int max_page=djvm_dir->get_pages_num()-1;
      for(GPosition pos=id_list.lastpos();pos;--pos)
      {
         GP<DjVmDir::File> frec=djvm_dir->id_to_file(id_list[pos]);
         if (frec)
         {
            int page_num=frec->get_page_num();
            int new_page_num=page_num+shift;
            if (new_page_num>max_page)
               new_page_num=max_page--;
            move_page(page_num, new_page_num);
         }
      }
   }
}

void
DjVuDocEditor::set_file_name(const GUTF8String &id, const GUTF8String &name)
{
   DEBUG_MSG("DjVuDocEditor::set_file_name(), id='" << id << "', name='" << name << "'\n");
   DEBUG_MAKE_INDENT(3);

      // It's important to get the URL now, because later (after we
      // change DjVmDir) id_to_url() will be returning a modified value
   GURL url=id_to_url(id);

      // Change DjVmDir. It will check if the name is unique
   djvm_dir->set_file_name(id, name);

      // Now find DjVuFile (if any) and rename it
   GPosition pos;
   if (files_map.contains(id, pos))
   {
      GP<File> file=files_map[pos];
      GP<DataPool> pool=file->pool;
      if (pool) pool->load_file();
      GP<DjVuFile> djvu_file=file->file;
      if (djvu_file) djvu_file->set_name(name);
   }
}

void
DjVuDocEditor::set_page_name(int page_num, const GUTF8String &name)
{
   DEBUG_MSG("DjVuDocEditor::set_page_name(), page_num='" << page_num << "'\n");
   DEBUG_MAKE_INDENT(3);

   if (page_num<0 || page_num>=get_pages_num())
      G_THROW( ERR_MSG("DjVuDocEditor.bad_page") "\t"+GUTF8String(page_num));

   set_file_name(page_to_id(page_num), name);
}

void
DjVuDocEditor::set_file_title(const GUTF8String &id, const GUTF8String &title)
{
   DEBUG_MSG("DjVuDocEditor::set_file_title(), id='" << id << "', title='" << title << "'\n");
   DEBUG_MAKE_INDENT(3);

      // Just change DjVmDir. It will check if the title is unique
   djvm_dir->set_file_title(id, title);
}

void
DjVuDocEditor::set_page_title(int page_num, const GUTF8String &title)
{
   DEBUG_MSG("DjVuDocEditor::set_page_title(), page_num='" << page_num << "'\n");
   DEBUG_MAKE_INDENT(3);

   if (page_num<0 || page_num>=get_pages_num())
      G_THROW( ERR_MSG("DjVuDocEditor.bad_page") "\t"+GUTF8String(page_num));

   set_file_title(page_to_id(page_num), title);
}

//****************************************************************************
//************************** Shared annotations ******************************
//****************************************************************************

void
DjVuDocEditor::simplify_anno(void (* progress_cb)(float progress, void *),
                             void * cl_data)
      // It's important that no decoding is done while this function
      // is running. Otherwise the DjVuFile's decoding routines and
      // this function may attempt to decode/modify a file's
      // annotations at the same time.
{
      // Get the name of the SHARED_ANNO file. We will not
      // touch that file (will not move annotations from it)
   GP<DjVmDir::File> shared_file=djvm_dir->get_shared_anno_file();
   GUTF8String shared_id;
   if (shared_file)
      shared_id=shared_file->get_load_name();

   GList<GURL> ignore_list;
   if (shared_id.length())
      ignore_list.append(id_to_url(shared_id));

      // First, for every page get merged (or "flatten" or "projected")
      // annotations and store them inside the top-level page file
   int pages_num=djvm_dir->get_pages_num();
   for(int page_num=0;page_num<pages_num;page_num++)
   {
      GP<DjVuFile> djvu_file=get_djvu_file(page_num);
      if (!djvu_file)
        G_THROW( ERR_MSG("DjVuDocEditor.page_fail") "\t" + GUTF8String(page_num));
      int max_level=0;
      GP<ByteStream> anno;
      anno=djvu_file->get_merged_anno(ignore_list, &max_level);
      if (anno && max_level>0)
      {
            // This is the moment when we try to modify DjVuFile's annotations
            // Make sure, that it's not being decoded
         GSafeFlags & file_flags=djvu_file->get_safe_flags();
         GMonitorLock lock(&file_flags);
         while(file_flags & DjVuFile::DECODING)
            file_flags.wait();
        
            // Merge all chunks in one by decoding and encoding DjVuAnno
         const GP<DjVuAnno> dec_anno(DjVuAnno::create());
         dec_anno->decode(anno);
         const GP<ByteStream> new_anno(ByteStream::create());
         dec_anno->encode(new_anno);
         new_anno->seek(0);

            // And store it in the file
         djvu_file->anno=new_anno;
         djvu_file->rebuild_data_pool();
         if ((file_flags & (DjVuFile::DECODE_OK |
                            DjVuFile::DECODE_FAILED |
                            DjVuFile::DECODE_STOPPED))==0)
            djvu_file->anno=0;
      }
      if (progress_cb)
    progress_cb((float)(page_num/2.0/pages_num), cl_data);
   }

      // Now remove annotations from every file except for
      // the top-level page files and SHARED_ANNO file.
      // Unlink empty files too.
   GPList<DjVmDir::File> files_list=djvm_dir->get_files_list();
   int cnt;
   GPosition pos;
   for(pos=files_list, cnt=0;pos;++pos, cnt++)
   {
      GP<DjVmDir::File> frec=files_list[pos];
      if (!frec->is_page() && frec->get_load_name()!=shared_id)
      {
         GP<DjVuFile> djvu_file=get_djvu_file(frec->get_load_name());
         if (djvu_file)
         {
            djvu_file->remove_anno();
            if (djvu_file->get_chunks_number()==0)
               remove_file(frec->get_load_name(), true);
         }
      }
      if (progress_cb)
         progress_cb((float)(0.5+cnt/2.0/files_list.size()), cl_data);
   }
}

void
DjVuDocEditor::create_shared_anno_file(void (* progress_cb)(float progress, void *),
                                       void * cl_data)
{
   if (djvm_dir->get_shared_anno_file())
      G_THROW( ERR_MSG("DjVuDocEditor.share_fail") );

      // Prepare file with ANTa chunk inside
   const GP<ByteStream> gstr(ByteStream::create());
   const GP<IFFByteStream> giff(IFFByteStream::create(gstr));
   IFFByteStream &iff=*giff;
   iff.put_chunk("FORM:DJVI");
   iff.put_chunk("ANTa");
   iff.close_chunk();
   iff.close_chunk();
   ByteStream &str=*gstr;
   str.flush();
   str.seek(0);
   const GP<DataPool> file_pool(DataPool::create(gstr));

      // Get a unique ID for the new file
   const GUTF8String id(find_unique_id("shared_anno.iff"));

      // Add it into the directory
   GP<DjVmDir::File> frec(DjVmDir::File::create(id, id, id,
     DjVmDir::File::SHARED_ANNO));
   djvm_dir->insert_file(frec, 1);

      // Add it to our "cache"
   {
      GP<File> f=new File;
      f->pool=file_pool;
      GCriticalSectionLock lock(&files_lock);
      files_map[id]=f;
   }

      // Now include this shared file into every top-level page file
   int pages_num=djvm_dir->get_pages_num();
   for(int page_num=0;page_num<pages_num;page_num++)
   {
      GP<DjVuFile> djvu_file=get_djvu_file(page_num);
      djvu_file->insert_file(id, 1);

      if (progress_cb)
         progress_cb((float) page_num/pages_num, cl_data);
   }
}

void 
DjVuDocEditor::set_djvm_nav(GP<DjVmNav> n)
{
  if (n && ! n->isValidBookmark())
    G_THROW("Invalid bookmark data");
  djvm_nav = n;
}

GP<DjVuFile>
DjVuDocEditor::get_shared_anno_file(void)
{
   GP<DjVuFile> djvu_file;

   GP<DjVmDir::File> frec=djvm_dir->get_shared_anno_file();
   if (frec)
      djvu_file=get_djvu_file(frec->get_load_name());

   return djvu_file;
}

GP<DataPool>
DjVuDocEditor::get_thumbnail(int page_num, bool dont_decode)
      // We override DjVuDocument::get_thumbnail() here because
      // pages may have been shuffled and those "thumbnail file records"
      // from the DjVmDir do not describe things correctly.
      //
      // So, first we will check the thumb_map[] if we have a predecoded
      // thumbnail for the given page. If this is the case, we will
      // return it. Otherwise we will ask DjVuDocument to generate
      // this thumbnail for us.
{
   const GUTF8String id(page_to_id(page_num));

   GCriticalSectionLock lock(&thumb_lock);
   const GPosition pos(thumb_map.contains(id));
   if (pos)
   {
         // Get the image from the map
      return thumb_map[pos];
   } else
   {
      unfile_thumbnails();
      return DjVuDocument::get_thumbnail(page_num, dont_decode);
   }
}

int
DjVuDocEditor::get_thumbnails_num(void) const
{
   GCriticalSectionLock lock((GCriticalSection *) &thumb_lock);

   int cnt=0;
   int pages_num=get_pages_num();
   for(int page_num=0;page_num<pages_num;page_num++)
   {
     if (thumb_map.contains(page_to_id(page_num)))
       cnt++;
   }
   return cnt;
}

int
DjVuDocEditor::get_thumbnails_size(void) const
{
   DEBUG_MSG("DjVuDocEditor::remove_thumbnails(): doing it\n");
   DEBUG_MAKE_INDENT(3);

   GCriticalSectionLock lock((GCriticalSection *) &thumb_lock);

   int pages_num=get_pages_num();
   for(int page_num=0;page_num<pages_num;page_num++)
   {
     const GPosition pos(thumb_map.contains(page_to_id(page_num)));
     if (pos)
     {
       const GP<ByteStream> gstr(thumb_map[pos]->get_stream());
       GP<IW44Image> iwpix=IW44Image::create_decode(IW44Image::COLOR);
       iwpix->decode_chunk(gstr);
      
       int width=iwpix->get_width();
       int height=iwpix->get_height();
       return width<height ? width : height;
    }
  }
  return -1;
}

void
DjVuDocEditor::remove_thumbnails(void)
{
   DEBUG_MSG("DjVuDocEditor::remove_thumbnails(): doing it\n");
   DEBUG_MAKE_INDENT(3);

   unfile_thumbnails();

   DEBUG_MSG("clearing thumb_map\n");
   GCriticalSectionLock lock(&thumb_lock);
   thumb_map.empty();
}

void
DjVuDocEditor::unfile_thumbnails(void)
      // Will erase all "THUMBNAILS" files from DjVmDir.
      // This function is useful when filing thumbnails (to get rid of
      // those files, which currently exist: they need to be replaced
      // anyway) and when calling DjVuDocument::get_thumbnail() to
      // be sure, that it will not use wrong information from DjVmDir
{
   DEBUG_MSG("DjVuDocEditor::unfile_thumbnails(): updating DjVmDir\n");
   DEBUG_MAKE_INDENT(3);

   {
     GCriticalSectionLock lock(&threqs_lock);
     threqs_list.empty();
   }
   if((const DjVmDir *)djvm_dir)
   {
     GPList<DjVmDir::File> xfiles_list=djvm_dir->get_files_list();
     for(GPosition pos=xfiles_list;pos;++pos)
     {
       GP<DjVmDir::File> f=xfiles_list[pos];
       if (f->is_thumbnails())
         djvm_dir->delete_file(f->get_load_name());
     }
   }
}

void
DjVuDocEditor::file_thumbnails(void)
      // The purpose of this function is to create files containing
      // thumbnail images and register them in DjVmDir.
      // If some of the thumbnail images are missing, they'll
      // be generated with generate_thumbnails()
{
   DEBUG_MSG("DjVuDocEditor::file_thumbnails(): updating DjVmDir\n");
   DEBUG_MAKE_INDENT(3);
   unfile_thumbnails();

      // Generate thumbnails if they're missing due to some reason.
   int thumb_num=get_thumbnails_num();
   int size=thumb_num>0 ? get_thumbnails_size() : 128;
   if (thumb_num!=get_pages_num())
   {
     generate_thumbnails(size);
   }

   DEBUG_MSG("filing thumbnails\n");

   GCriticalSectionLock lock(&thumb_lock);

      // The first thumbnail file always contains only one thumbnail
   int ipf=1;
   int image_num=0;
   int page_num=0, pages_num=djvm_dir->get_pages_num();
   GP<ByteStream> str(ByteStream::create());
   GP<IFFByteStream> iff(IFFByteStream::create(str));
   iff->put_chunk("FORM:THUM");
   for(;;)
   {
      GUTF8String id(page_to_id(page_num));
      const GPosition pos(thumb_map.contains(id));
      if (! pos)
      {
        G_THROW( ERR_MSG("DjVuDocEditor.no_thumb") "\t"+GUTF8String(page_num));
      }
      iff->put_chunk("TH44");
      iff->copy(*(thumb_map[pos]->get_stream()));
      iff->close_chunk();
      image_num++;
      page_num++;
      if (image_num>=ipf || page_num>=pages_num)
      {
         int i=id.rsearch('.');
         if(i<=0)
         {
           i=id.length();
         }
         id=id.substr(0,i)+".thumb";
            // Get unique ID for this file
         id=find_unique_id(id);

            // Create a file record with the chosen ID
         GP<DjVmDir::File> file(DjVmDir::File::create(id, id, id,
           DjVmDir::File::THUMBNAILS));

            // Set correct file position (so that it will cover the next
            // ipf pages)
         int file_pos=djvm_dir->get_page_pos(page_num-image_num);
         djvm_dir->insert_file(file, file_pos);

            // Now add the File record (containing the file URL and DataPool)
            // After we do it a simple save_as() will save the document
            // with the thumbnails. This is because DjVuDocument will see
            // the file in DjVmDir and will ask for data. We will intercept
            // the request for data and will provide this DataPool
         iff->close_chunk();
         str->seek(0);
         const GP<DataPool> file_pool(DataPool::create(str));
         GP<File> f=new File;
         f->pool=file_pool;
         GCriticalSectionLock lock(&files_lock);
         files_map[id]=f;

            // And create new streams
         str=ByteStream::create();
         iff=IFFByteStream::create(str);
         iff->put_chunk("FORM:THUM");
         image_num=0;

            // Reset ipf to correct value (after we stored first
            // "exceptional" file with thumbnail for the first page)
         if (page_num==1) ipf=thumbnails_per_file;
         if (page_num>=pages_num) break;
      }
   }
}

int
DjVuDocEditor::generate_thumbnails(int thumb_size, int page_num)
{
   DEBUG_MSG("DjVuDocEditor::generate_thumbnails(): doing it\n");
   DEBUG_MAKE_INDENT(3);

   if(page_num<(djvm_dir->get_pages_num()))
   {
      const GUTF8String id(page_to_id(page_num));
      if (!thumb_map.contains(id))
        {
          const GP<DjVuImage> dimg(get_page(page_num, true));
         
          GRect rect(0, 0, thumb_size, dimg->get_height()*thumb_size/dimg->get_width());
          GP<GPixmap> pm=dimg->get_pixmap(rect, rect, get_thumbnails_gamma());
          if (!pm)
            {
              const GP<GBitmap> bm(dimg->get_bitmap(rect, rect, sizeof(int)));
              if (bm) 
                pm = GPixmap::create(*bm);
              else
                pm = GPixmap::create(rect.height(), rect.width(), &GPixel::WHITE);
            }
          // Store and compress the pixmap
          const GP<IW44Image> iwpix(IW44Image::create_encode(*pm));
          const GP<ByteStream> gstr(ByteStream::create());
          IWEncoderParms parms;
          parms.slices=97;
          parms.bytes=0;
          parms.decibels=0;
          iwpix->encode_chunk(gstr, parms);
          gstr->seek(0L);
          thumb_map[id]=DataPool::create(gstr);
        }
      ++page_num;
   }
   else
   {
     page_num = -1;
   }
   return page_num;
}

void
DjVuDocEditor::generate_thumbnails(int thumb_size,
                                   bool (* cb)(int page_num, void *),
                                   void * cl_data)
{
   int page_num=0;
   do
   {
     page_num=generate_thumbnails(thumb_size,page_num);
     if (cb && page_num>0) if (cb(page_num-1, cl_data)) return;
   } while(page_num>=0);
}

static void
store_file(const GP<DjVmDir> & src_djvm_dir, const GP<DjVmDoc> & djvm_doc,
           GP<DjVuFile> & djvu_file, GMap<GURL, void *> & map)
{
   GURL url=djvu_file->get_url();
   if (!map.contains(url))
   {
      map[url]=0;

         // Store included files first
      GPList<DjVuFile> djvu_files_list=djvu_file->get_included_files(false);
      for(GPosition pos=djvu_files_list;pos;++pos)
         store_file(src_djvm_dir, djvm_doc, djvu_files_list[pos], map);

         // Now store contents of this file
      GP<DataPool> file_data=djvu_file->get_djvu_data(false);
      GP<DjVmDir::File> frec=src_djvm_dir->name_to_file(url.name());
      if (frec)
      {
         frec=new DjVmDir::File(*frec);
         djvm_doc->insert_file(frec, file_data, -1);
      }
   }
}

void
DjVuDocEditor::save_pages_as(
  const GP<ByteStream> &str, const GList<int> & _page_list)
{
   GList<int> page_list=sortList(_page_list);

   GP<DjVmDoc> djvm_doc=DjVmDoc::create();
   GMap<GURL, void *> map;
   for(GPosition pos=page_list;pos;++pos)
   {
      GP<DjVmDir::File> frec=djvm_dir->page_to_file(page_list[pos]);
      if (frec)
      {
         GP<DjVuFile> djvu_file=get_djvu_file(frec->get_load_name());
         if (djvu_file)
            store_file(djvm_dir, djvm_doc, djvu_file, map);
      }
   }
   djvm_doc->write(str);
}

void
DjVuDocEditor::save_file(const GUTF8String &file_id, const GURL &codebase,
  const bool only_modified, GMap<GUTF8String,GUTF8String> & map)
{
  if(only_modified)
  {
    for(GPosition pos=files_map;pos;++pos)
    {
      const GP<File> file_rec(files_map[pos]);
      const bool file_modified=file_rec->pool ||
        (file_rec->file && file_rec->file->is_modified());
      if(!file_modified)
      {
        const GUTF8String id=files_map.key(pos);
        const GUTF8String save_name(djvm_dir->id_to_file(id)->get_save_name());
        if(id == save_name)
        {
          map[id]=id;
        }
      }
    }
  }
  save_file(file_id,codebase,map);
}

void
DjVuDocEditor::save_file(
  const GUTF8String &file_id, const GURL &codebase,
  GMap<GUTF8String,GUTF8String> & map)
{
   DEBUG_MSG("DjVuDocEditor::save_file(): ID='" << file_id << "'\n");
   DEBUG_MAKE_INDENT(3);

   if (!map.contains(file_id))
   {
      const GP<DjVmDir::File> file(djvm_dir->id_to_file(file_id));

      GP<DataPool> file_pool;
      const GPosition pos(files_map.contains(file_id));
      if (pos)
      {
         const GP<File> file_rec(files_map[pos]);
         if (file_rec->file)
            file_pool=file_rec->file->get_djvu_data(false);
         else
            file_pool=file_rec->pool;
      }

      if (!file_pool)
      {
         DjVuPortcaster * pcaster=DjVuPort::get_portcaster();
         file_pool=pcaster->request_data(this, id_to_url(file_id));
      }

      if (file_pool)
      {
         GMap<GUTF8String,GUTF8String> incl;
         map[file_id]=get_djvm_doc()->save_file(codebase,*file,incl,file_pool);
         for(GPosition pos=incl;pos;++pos)
         {
           save_file(incl.key(pos),codebase ,map);
         }
      }else
      {
        map[file_id]=file->get_save_name();
      }
   }
}

void
DjVuDocEditor::save(void)
{
   DEBUG_MSG("DjVuDocEditor::save(): saving the file\n");
   DEBUG_MAKE_INDENT(3);

   if (!can_be_saved())
     G_THROW( ERR_MSG("DjVuDocEditor.cant_save") );
   save_as(GURL(), orig_doc_type!=INDIRECT);
}

void
DjVuDocEditor::write(const GP<ByteStream> &gbs, bool force_djvm)
{
  DEBUG_MSG("DjVuDocEditor::write()\n");
  DEBUG_MAKE_INDENT(3);
  if (get_thumbnails_num()==get_pages_num())
  {
    file_thumbnails();
  }else
  { 
    remove_thumbnails();
  }
  clean_files_map();
  DjVuDocument::write(gbs,force_djvm);
}

void
DjVuDocEditor::write(
  const GP<ByteStream> &gbs,const GMap<GUTF8String,void *> &reserved)
{
  DEBUG_MSG("DjVuDocEditor::write()\n");
  DEBUG_MAKE_INDENT(3);
  if (get_thumbnails_num()==get_pages_num())
  {
    file_thumbnails();
  }else
  { 
    remove_thumbnails();
  }
  clean_files_map();
  DjVuDocument::write(gbs,reserved);
}

void
DjVuDocEditor::save_as(const GURL &where, bool bundled)
{
   DEBUG_MSG("DjVuDocEditor::save_as(): where='" << where << "'\n");
   DEBUG_MAKE_INDENT(3);

      // First see if we need to generate (or just reshuffle) thumbnails...
      // If we have an icon for every page, we will just call
      // file_thumbnails(), which will update DjVmDir and will create
      // the actual bundles with thumbnails (very fast)
      // Otherwise we will remove the thumbnails completely because
      // we really don't want to deal with documents, which have only
      // some of their pages thumbnailed.
   if (get_thumbnails_num()==get_pages_num())
   {
     file_thumbnails();
   }else
   { 
     remove_thumbnails();
   }

   GURL save_doc_url;

   if (where.is_empty())
   {
         // Assume, that we just want to 'save'. Check, that it's possible
         // and proceed.
      bool can_be_saved_bundled =
	orig_doc_type==BUNDLED ||
	orig_doc_type==OLD_BUNDLED ||
	orig_doc_type==SINGLE_PAGE ||
	(orig_doc_type==OLD_INDEXED && orig_doc_pages==1);
      if ((bundled ^ can_be_saved_bundled)!=0)
         G_THROW( ERR_MSG("DjVuDocEditor.cant_save2") );
      save_doc_url=doc_url;
   } else
   {
      save_doc_url=where;
   }

   int save_doc_type=bundled ? BUNDLED : INDIRECT;

   clean_files_map();

   GCriticalSectionLock lock(&files_lock);

   DjVuPortcaster * pcaster=DjVuPort::get_portcaster();

      // First consider saving in SINGLE_FILE format (one file)
   if(needs_compression())
   {
     DEBUG_MSG("Compressing on output\n");
     remove_thumbnails();
     if(! djvu_compress_codec)
     {
       G_THROW( ERR_MSG("DjVuDocEditor.no_codec") );
     }
     const GP<DjVmDoc> doc(get_djvm_doc());
     GP<ByteStream> mbs(ByteStream::create());
     doc->write(mbs);
     mbs->flush();
     mbs->seek(0,SEEK_SET);
     djvu_compress_codec(mbs,save_doc_url,(!(const DjVmDir *)djvm_dir)||(djvm_dir->get_files_num()==1)||(save_doc_type!=INDIRECT));
     files_map.empty();
     doc_url=GURL();
   }else
   {
     bool singlepage = (djvm_dir->get_files_num()==1 && !djvm_nav);
     if (singlepage)
     {
       // maybe save as single page
       DjVmDir::File *file = djvm_dir->page_to_file(0);
       if (file->get_title() != file->get_load_name())
         singlepage = false;
     }
     if (singlepage)
     {
       // Here 'bundled' has no effect: we will save it as one page.
       DEBUG_MSG("saving one file...\n");
       GURL file_url=page_to_url(0);
       const GUTF8String file_id(djvm_dir->page_to_file(0)->get_load_name());
       GP<DataPool> file_pool;
       GPosition pos=files_map.contains(file_id);
       if (pos)
       {
         const GP<File> file_rec(files_map[pos]);
         if (file_rec->pool && (!file_rec->file ||
                                !file_rec->file->is_modified()))
         {
           file_pool=file_rec->pool;
         }else if (file_rec->file)
         {
           file_pool=file_rec->file->get_djvu_data(false);
         }
       }
       // Even if file has not been modified (pool==0) we still want
       // to save it.
       if (!file_pool)
         file_pool=pcaster->request_data(this, file_url);
       if (file_pool)
       {
         DEBUG_MSG("Saving '" << file_url << "' to '" << save_doc_url << "'\n");
         DataPool::load_file(save_doc_url);
         const GP<ByteStream> gstr_out(ByteStream::create(save_doc_url, "wb"));
         ByteStream &str_out=*gstr_out;
         str_out.writall(octets, 4);
         const GP<ByteStream> str_in(file_pool->get_stream());
         str_out.copy(*str_in);
       }

       // Update the document's DataPool (to save memory)
       const GP<DjVmDoc> doc(get_djvm_doc());
       const GP<ByteStream> gstr=ByteStream::create();// One page: we can do it in the memory
       doc->write(gstr);
       gstr->seek(0, SEEK_SET);
       const GP<DataPool> pool(DataPool::create(gstr));
       doc_pool=pool;
       init_data_pool=pool;

         // Also update DjVmDir (to reflect changes in offsets)
       djvm_dir=doc->get_djvm_dir();
     } else if (save_doc_type==INDIRECT)
     {
       DEBUG_MSG("Saving in INDIRECT format to '" << save_doc_url << "'\n");
       bool save_only_modified=!(save_doc_url!=doc_url || save_doc_type!=orig_doc_type);
       GPList<DjVmDir::File> xfiles_list=djvm_dir->resolve_duplicates(false);
       const GURL codebase=save_doc_url.base();
       int pages_num=djvm_dir->get_pages_num();
       GMap<GUTF8String, GUTF8String> map;
       // First go thru the pages
       for(int page_num=0;page_num<pages_num;page_num++)
       {
         const GUTF8String id(djvm_dir->page_to_file(page_num)->get_load_name());
         save_file(id, codebase, save_only_modified, map);
       }
       // Next go thru thumbnails and similar stuff
       GPosition pos;
       for(pos=xfiles_list;pos;++pos)
         save_file(xfiles_list[pos]->get_load_name(), codebase, save_only_modified, map);

         // Finally - save the top-level index file
       for(pos=xfiles_list;pos;++pos)
       {
         const GP<DjVmDir::File> file(xfiles_list[pos]);
         file->offset=0;
         file->size=0;
       }
       DataPool::load_file(save_doc_url);
       const GP<ByteStream> gstr(ByteStream::create(save_doc_url, "wb"));
       const GP<IFFByteStream> giff(IFFByteStream::create(gstr));
       IFFByteStream &iff=*giff;

       iff.put_chunk("FORM:DJVM", 1);
       iff.put_chunk("DIRM");
       djvm_dir->encode(giff->get_bytestream());
       iff.close_chunk();
       if (djvm_nav)
         {
           iff.put_chunk("NAVM");
           djvm_nav->encode(iff.get_bytestream());
           iff.close_chunk();
         }
       iff.close_chunk();
       iff.flush();

       // Update the document data pool (not required, but will save memory)
       doc_pool=DataPool::create(save_doc_url);
       init_data_pool=doc_pool;

       // No reason to update DjVmDir as for this format it doesn't
       // contain DJVM offsets
     } else if (save_doc_type==BUNDLED || save_doc_type==OLD_BUNDLED)
     {
        DEBUG_MSG("Saving in BUNDLED format to '" << save_doc_url << "'\n");

         // Can't be very smart here. Simply overwrite the file.
        const GP<DjVmDoc> doc(get_djvm_doc());
        DataPool::load_file(save_doc_url);
        const GP<ByteStream> gstr(ByteStream::create(save_doc_url, "wb"));
        doc->write(gstr);
        gstr->flush();

         // Update the document data pool (not required, but will save memory)
        doc_pool=DataPool::create(save_doc_url);
        init_data_pool=doc_pool;

         // Also update DjVmDir (to reflect changes in offsets)
        djvm_dir=doc->get_djvm_dir();
     } else
     {
       G_THROW( ERR_MSG("DjVuDocEditor.cant_save") );
     }

        // Now, after we have saved the document w/o any error, detach DataPools,
        // which are in the 'File's list to save memory. Detach everything.
        // Even in the case when File->file is non-zero. If File->file is zero,
        // remove the item from the list at all. If it's non-zero, it has
        // to stay there because by definition files_map[] contains the list
        // of all active files and customized DataPools
        //
        // In addition to it, look thru all active files and change their URLs
        // to reflect changes in the document's URL (if there was a change)
        // Another reason why file's URLs must be changed is that we may have
        // saved the document in a different format, which changes the rules
        // of file url composition.
     for(GPosition pos=files_map;pos;)
       {
	 const GP<File> file_rec(files_map[pos]);
	 file_rec->pool=0;
	 if (file_rec->file==0)
	   {
	     GPosition this_pos=pos;
	     ++pos;
	     files_map.del(this_pos);
	   } else
	   {
	     // Change the file's url;
	     if (doc_url!=save_doc_url ||
		 orig_doc_type!=save_doc_type)
	       {
		 if (save_doc_type==BUNDLED)
		   file_rec->file->move(save_doc_url);
		 else
		   file_rec->file->move(save_doc_url.base());
	       }
	     ++pos;
	   }
       }
   }
   orig_doc_type=save_doc_type;
   doc_type=save_doc_type;

   if (doc_url!=save_doc_url)
   {
     // Also update document's URL (we moved, didn't we?)
     doc_url=save_doc_url;
     init_url=save_doc_url;
   }
}

GP<DjVuDocEditor> 
DjVuDocEditor::create_wait(void)
{
  DjVuDocEditor *doc=new DjVuDocEditor();
  const GP<DjVuDocEditor> retval(doc);
  doc->init();
  return retval;
}

GP<DjVuDocEditor> 
DjVuDocEditor::create_wait(const GURL &url)
{
  DjVuDocEditor *doc=new DjVuDocEditor();
  const GP<DjVuDocEditor> retval(doc);
  doc->init(url);
  return retval;
}

bool
DjVuDocEditor::inherits(const GUTF8String &class_name) const
{
   return (class_name == "DjVuDocEditor")||DjVuDocument::inherits(class_name);
}

int
DjVuDocEditor::get_orig_doc_type(void) const
{
   return orig_doc_type;
}

bool
DjVuDocEditor::can_be_saved(void) const
{
   return !(needs_rename()||needs_compression()||orig_doc_type==UNKNOWN_TYPE ||
	    orig_doc_type==OLD_INDEXED);
}

int
DjVuDocEditor::get_save_doc_type(void) const
{
   if (orig_doc_type==SINGLE_PAGE)
      if (djvm_dir->get_files_num()==1)
        return SINGLE_PAGE;
      else
        return BUNDLED;
   else if (orig_doc_type==INDIRECT)
     return INDIRECT;
   else if (orig_doc_type==OLD_BUNDLED || orig_doc_type==BUNDLED)
     return BUNDLED;
   else
     return UNKNOWN_TYPE;
}

GURL
DjVuDocEditor::get_doc_url(void) const
{
   return doc_url.is_empty() ? init_url : doc_url;
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
