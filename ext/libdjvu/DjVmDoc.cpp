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

#include "DjVmDoc.h"
#include "DjVmNav.h"
#include "DataPool.h"
#include "IFFByteStream.h"
#include "GOS.h"
#include "debug.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

static const char octets[4]={0x41,0x54,0x26,0x54};

// Save the file to disk, remapping INCL chunks while saving.
static void
save_file(
  IFFByteStream &iff_in, IFFByteStream &iff_out, const DjVmDir &dir,
  GMap<GUTF8String,GUTF8String> &incl)
{
  GUTF8String chkid;
  if (iff_in.get_chunk(chkid))
  {
    iff_out.put_chunk(chkid,true);
    if(!chkid.cmp("FORM:",5))
    {
      for(;iff_in.get_chunk(chkid);iff_in.close_chunk())
      {
        iff_out.put_chunk(chkid);
        if(chkid == "INCL")
        {
          GUTF8String incl_str;
          char buffer[1024];
          int length;
          while((length=iff_in.read(buffer, 1024)))
            incl_str+=GUTF8String(buffer, length);
          // Eat '\n' in the beginning and at the end
          while(incl_str.length() && incl_str[0]=='\n')
          {
            incl_str=incl_str.substr(1,(unsigned int)(-1));
          }
          while(incl_str.length()>0 && incl_str[(int)incl_str.length()-1]=='\n')
          {
            incl_str.setat(incl_str.length()-1, 0);
          }
          GPosition pos=incl.contains(incl_str);
          if(pos)
          { 
            iff_out.get_bytestream()->writestring(incl[pos]);
          }else
          {
            GP<DjVmDir::File> incl_file=dir.id_to_file(incl_str); 
            if(incl_file)
            {
              DEBUG_MSG("INCL '"<<(const char *)incl_file->get_save_name()<<"'\n");
              const GUTF8String incl_name=incl_file->get_save_name();
              incl[incl_str]=incl_name;
              iff_out.get_bytestream()->writestring(incl_name);
            }else
            {
              DEBUG_MSG("BOGUS INCL '"<<(const char *)incl_str<<"'\n");
              iff_out.copy(*iff_in.get_bytestream());
            }
          }
        }else
        {
          iff_out.copy(*iff_in.get_bytestream());
        }
        iff_out.close_chunk();
      }
    }else
    {
      iff_out.copy(*iff_in.get_bytestream());
    }
    iff_out.close_chunk();
    iff_in.close_chunk();
  }
}

DjVmDoc::DjVmDoc(void)
{
   DEBUG_MSG("DjVmDoc::DjVmDoc(): Constructing empty DjVm document.\n");
   DEBUG_MAKE_INDENT(3);
}

void
DjVmDoc::init(void)
{
  dir=DjVmDir::create();
}

GP<DjVmDoc>
DjVmDoc::create(void)
{
  DjVmDoc *doc=new DjVmDoc();
  GP<DjVmDoc> retval=doc;
  doc->init();
  return retval;
}

void
DjVmDoc::insert_file(const GP<DjVmDir::File> & f,
		     GP<DataPool> data_pool, int pos)
{
   DEBUG_MSG("DjVmDoc::insert_file(): inserting file '" << f->get_load_name() <<
	     "' at pos " << pos << "\n");
   DEBUG_MAKE_INDENT(3);

   if (!f)
     G_THROW( ERR_MSG("DjVmDoc.no_zero_file") );
   if (data.contains(f->get_load_name()))
     G_THROW( ERR_MSG("DjVmDoc.no_duplicate") );

   char buffer[4];
   if (data_pool->get_data(buffer, 0, 4)==4 && !memcmp(buffer, octets, 4))
   {
      data_pool=DataPool::create(data_pool, 4, -1);
   } 
   data[f->get_load_name()]=data_pool;
   dir->insert_file(f, pos);
}

void
DjVmDoc::insert_file(
  ByteStream &data, DjVmDir::File::FILE_TYPE file_type,
  const GUTF8String &name, const GUTF8String &id, const GUTF8String &title,
  int pos)
{
   const GP<DjVmDir::File> file(
     DjVmDir::File::create(name, id, title, file_type));
   const GP<DataPool> pool(DataPool::create());
      // Cannot connect to a bytestream.
      // Must copy data into the datapool.
   int nbytes;
   char buffer[1024];
   while ((nbytes = data.read(buffer, sizeof(buffer))))
      pool->add_data(buffer, nbytes);
   pool->set_eof();
      // Call low level insert
   insert_file(file, pool, pos);
}

void
DjVmDoc::insert_file(
  const GP<DataPool> &pool, DjVmDir::File::FILE_TYPE file_type,
  const GUTF8String &name, const GUTF8String &id, const GUTF8String &title,
  int pos)
{
   const GP<DjVmDir::File> file(
     DjVmDir::File::create(name, id, title, file_type));
      // Call low level insert
   insert_file(file, pool, pos);
}

void
DjVmDoc::delete_file(const GUTF8String &id)
{
   DEBUG_MSG("DjVmDoc::delete_file(): deleting file '" << id << "'\n");
   DEBUG_MAKE_INDENT(3);
   
   if (!data.contains(id))
      G_THROW(GUTF8String( ERR_MSG("DjVmDoc.cant_delete") "\t") + id);
   
   data.del(id);
   dir->delete_file(id);
}

void 
DjVmDoc::set_djvm_nav(GP<DjVmNav> n)
{
  if (n && ! n->isValidBookmark())
    G_THROW("Invalid bookmark data");
  nav = n;
}

GP<DataPool>
DjVmDoc::get_data(const GUTF8String &id) const
{
  GPosition pos;
  if (!data.contains(id, pos))
    G_THROW(GUTF8String( ERR_MSG("DjVmDoc.cant_find") "\t") + id);
  const GP<DataPool> pool(data[pos]);
   // First check that the file is in IFF format
  G_TRY
  {
    const GP<ByteStream> str_in(pool->get_stream());
    const GP<IFFByteStream> giff_in=IFFByteStream::create(str_in);
    IFFByteStream &iff_in=*giff_in;
    GUTF8String chkid;
    int size=iff_in.get_chunk(chkid);
    if (size<0 || size>0x7fffffff)
      G_THROW( ERR_MSG("DjVmDoc.not_IFF") "\t" + id);
  }
  G_CATCH_ALL 
  {
    G_THROW( ERR_MSG("DjVmDoc.not_IFF") "\t" + id);
  }
  G_ENDCATCH;
  return pool;
}

void
DjVmDoc::write(const GP<ByteStream> &gstr)
{
  const GMap<GUTF8String,void *> reserved;
  write(gstr,reserved);
}

static inline GUTF8String
get_name(const DjVmDir::File &file)
{
  const GUTF8String save_name(file.get_save_name());
  return save_name.length()?save_name:(file.get_load_name());
}

void
DjVmDoc::write(const GP<ByteStream> &gstr,
               const GMap<GUTF8String,void *> &reserved)
{
  DEBUG_MSG("DjVmDoc::write(): Storing document into the byte stream.\n");
  DEBUG_MAKE_INDENT(3);

  GPList<DjVmDir::File> files_list=dir->resolve_duplicates(true);
  bool do_rename=false;
  GPosition pos(reserved);

  GMap<GUTF8String,GUTF8String> incl;
  DEBUG_MSG("pass 1: looking for reserved names.");
  if(pos)
  {
      // Check if there are any conflicting file names.
    for(pos=files_list;pos;++pos)
    {
      GP<DjVmDir::File> file=files_list[pos];
      if((do_rename=(reserved.contains(file->get_load_name())?true:false))
		  ||(do_rename=(reserved.contains(file->get_save_name())?true:false)))
      {
        break;
      }
    }
    // If there are conflicting file names, check if the save names
    // are OK.  If not, generate new save names.
    if(do_rename)
    {
      DEBUG_MSG("pass 1: renaming reserved names.");
      for(;;files_list=dir->resolve_duplicates(true))
      {
        GMap<GUTF8String,void *> this_doc;
        for(pos=files_list;pos;++pos)
        {
          GP<DjVmDir::File> file=files_list[pos];
          this_doc[::get_name(*file)]=0;
        }
        bool need_new_list=false;
        for(pos=files_list;pos;++pos)
        {
          GP<DjVmDir::File> file=files_list[pos];
          const GUTF8String name(::get_name(*file));
          if(reserved.contains(name))
          {
            GUTF8String new_name;
            int series=0;
            do
            {
              int dot=name.rsearch('.');
              if(dot>0)
              {
                new_name=name.substr(0,dot)+
                  "_"+GUTF8String(++series)+name.substr(dot,-1);
              }else
              {
                new_name=name+"_"+GUTF8String(++series);
              }
            } while(reserved.contains(new_name)||this_doc.contains(new_name));
            dir->set_file_name(file->get_load_name(),new_name);
            need_new_list=true;
          }
        }
        if(!need_new_list)
          break;
      }
    }
  }

  DEBUG_MSG("pass 2: create dummy DIRM chunk and calculate offsets...\n");
  for(pos=files_list;pos;++pos)
  {
    GP<DjVmDir::File> file=files_list[pos];
    file->offset=0xffffffff;
    GPosition data_pos=data.contains(file->get_load_name());
    if (!data_pos)
      G_THROW( ERR_MSG("DjVmDoc.no_data") "\t" + file->get_load_name());
    if(do_rename)
    {
      GP<ByteStream> gout(ByteStream::create());
      {
        const GP<IFFByteStream> giff_in(
          IFFByteStream::create(data[data_pos]->get_stream()));
        const GP<IFFByteStream> giff_out(IFFByteStream::create(gout));
        ::save_file(*giff_in,*giff_out,*dir,incl);
      }
      gout->seek(0L);
      data[data_pos]=DataPool::create(gout);
    }
    file->size=data[data_pos]->get_length();
    if (!file->size)
      G_THROW( ERR_MSG("DjVmDoc.zero_file") );
  }
   
  const GP<ByteStream> tmp_str(ByteStream::create());
  const GP<IFFByteStream> gtmp_iff(IFFByteStream::create(tmp_str));
  IFFByteStream &tmp_iff=*gtmp_iff;
  tmp_iff.put_chunk("FORM:DJVM", 1);
  tmp_iff.put_chunk("DIRM");
  dir->encode(tmp_iff.get_bytestream(),do_rename);
  tmp_iff.close_chunk();
  if (nav)
    {
      tmp_iff.put_chunk("NAVM");
      nav->encode(tmp_iff.get_bytestream());
      tmp_iff.close_chunk();
    }
  tmp_iff.close_chunk();
  int offset=tmp_iff.tell();

  for(pos=files_list;pos;++pos)
  {
    if ((offset & 1)!=0)
      offset++;
      
    GP<DjVmDir::File> & file=files_list[pos];
    file->offset=offset;
    offset+=file->size;	// file->size has been set in the first pass
  }

  DEBUG_MSG("pass 3: store the file contents.\n");

  GP<IFFByteStream> giff=IFFByteStream::create(gstr);
  IFFByteStream &iff=*giff;
  iff.put_chunk("FORM:DJVM", 1);
  iff.put_chunk("DIRM");
  dir->encode(iff.get_bytestream(),do_rename);
  iff.close_chunk();
  if (nav)
    {
      iff.put_chunk("NAVM");
      nav->encode(iff.get_bytestream());
      iff.close_chunk();
    }

  for(pos=files_list;pos;++pos)
  {
    GP<DjVmDir::File> & file=files_list[pos];

    const GP<DataPool> pool=get_data(file->get_load_name());
    const GP<ByteStream> str_in(pool->get_stream());
    if ((iff.tell() & 1)!=0)
    {
      iff.get_bytestream()->write8(0);
    }
    iff.copy(*str_in);
  }

  iff.close_chunk();
  iff.flush();

  DEBUG_MSG("done storing DjVm file.\n");
}

void
DjVmDoc::read(const GP<DataPool> & pool)
{
   DEBUG_MSG("DjVmDoc::read(): reading the BUNDLED doc contents from the pool\n");
   DEBUG_MAKE_INDENT(3);
   
   const GP<ByteStream> str(pool->get_stream());
   
   GP<IFFByteStream> giff=IFFByteStream::create(str);
   IFFByteStream &iff=*giff;
   GUTF8String chkid;
   iff.get_chunk(chkid);
   if (chkid!="FORM:DJVM")
      G_THROW( ERR_MSG("DjVmDoc.no_form_djvm") );

   iff.get_chunk(chkid);
   if (chkid!="DIRM")
      G_THROW( ERR_MSG("DjVmDoc.no_dirm_chunk") );
   dir->decode(iff.get_bytestream());
   iff.close_chunk();

   data.empty();

   if (dir->is_indirect())
      G_THROW( ERR_MSG("DjVmDoc.cant_read_indr") );

   GPList<DjVmDir::File> files_list=dir->get_files_list();
   for(GPosition pos=files_list;pos;++pos)
   {
      DjVmDir::File * f=files_list[pos];
      
      DEBUG_MSG("reading contents of file '" << f->get_load_name() << "'\n");
      data[f->get_load_name()]=DataPool::create(pool, f->offset, f->size);
   }
}

void
DjVmDoc::read(ByteStream & str_in)
{
   DEBUG_MSG("DjVmDoc::read(): reading the BUNDLED doc contents from the stream\n");
   DEBUG_MAKE_INDENT(3);

   GP<DataPool> pool=DataPool::create();
   char buffer[1024];
   int length;
   while((length=str_in.read(buffer, 1024)))
      pool->add_data(buffer, length);
   pool->set_eof();

   read(pool);
}

void
DjVmDoc::read(const GURL &url)
{
   DEBUG_MSG("DjVmDoc::read(): reading the doc contents from the HDD\n");
   DEBUG_MAKE_INDENT(3);

   GP<DataPool> pool=DataPool::create(url);
   const GP<ByteStream> str(pool->get_stream());
   GP<IFFByteStream> giff=IFFByteStream::create(str);
   IFFByteStream &iff=*giff;
   GUTF8String chkid;
   iff.get_chunk(chkid);
   if (chkid!="FORM:DJVM")
      G_THROW( ERR_MSG("DjVmDoc.no_form_djvm2") );

   iff.get_chunk(chkid);
   if (chkid!="DIRM")
      G_THROW( ERR_MSG("DjVmDoc.no_dirm_chunk") );
   dir->decode(iff.get_bytestream());
   iff.close_chunk();

   if (dir->is_bundled())
     read(pool);
   else
   {
//      GUTF8String full_name=GOS::expand_name(name);
//      GUTF8String dir_name=GOS::dirname(GOS::url_to_filename(url.base()));
      GURL dirbase=url.base();

      data.empty();

      GPList<DjVmDir::File> files_list=dir->get_files_list();
      for(GPosition pos=files_list;pos;++pos)
      {
	 DjVmDir::File * f=files_list[pos];
      
	 DEBUG_MSG("reading contents of file '" << f->get_load_name() << "'\n");

         const GURL::UTF8 url(f->get_load_name(),dirbase);
	 data[f->get_load_name()]=DataPool::create(url);
      }
   }
}

void
DjVmDoc::write_index(const GP<ByteStream> &str)
{
   DEBUG_MSG("DjVmDoc::write_index(): Storing DjVm index file\n");
   DEBUG_MAKE_INDENT(3);

   GPList<DjVmDir::File> files_list=dir->get_files_list();
   for(GPosition pos=files_list;pos;++pos)
   {
      GP<DjVmDir::File> file=files_list[pos];
      file->offset=0;

      GPosition data_pos=data.contains(file->get_load_name());
      if (!data_pos)
	G_THROW( ERR_MSG("DjVmDoc.no_data") "\t" + file->get_load_name());
      file->size=data[data_pos]->get_length();
      if (!file->size)
        G_THROW( ERR_MSG("DjVmDoc.zero_file") );
   }

   GP<IFFByteStream> giff=IFFByteStream::create(str);
   IFFByteStream &iff=*giff;
   iff.put_chunk("FORM:DJVM", 1);
   iff.put_chunk("DIRM");
   dir->encode(iff.get_bytestream());
   iff.close_chunk();
   if (nav)
     {
       iff.put_chunk("NAVM");
       nav->encode(iff.get_bytestream());
       iff.close_chunk();
     }
   iff.close_chunk();
   iff.flush();
}

void
DjVmDoc::save_page(
  const GURL &codebase, const DjVmDir::File &file) const
{
  GMap<GUTF8String,GUTF8String> incl;
  save_file(codebase,file,&incl);
}

void
DjVmDoc::save_page(
  const GURL &codebase, const DjVmDir::File &file,
  GMap<GUTF8String,GUTF8String> &incl ) const
{
  save_file(codebase,file,&incl);
}

void
DjVmDoc::save_file(
  const GURL &codebase, const DjVmDir::File &file) const
{
  save_file(codebase,file,0);
}

GUTF8String 
DjVmDoc::save_file(const GURL &codebase, const DjVmDir::File &file,
  GMap<GUTF8String,GUTF8String> &incl, const GP<DataPool> &pool) const
{
  const GUTF8String save_name(file.get_save_name());
  const GURL::UTF8 new_url(save_name,codebase);
  DEBUG_MSG("storing file '"<<new_url<<"'\n");
  DataPool::load_file(new_url);
  const GP<ByteStream> str_in(pool->get_stream());
  const GP<ByteStream> str_out(ByteStream::create(new_url, "wb"));
  ::save_file( *IFFByteStream::create(str_in),
      *IFFByteStream::create(str_out), *dir, incl);
  return save_name;
}

void
DjVmDoc::save_file(
  const GURL &codebase, const DjVmDir::File &file,
  GMap<GUTF8String,GUTF8String> *incl) const
{
  const GUTF8String load_name=file.get_load_name();
  if(!incl || !incl->contains(load_name))
  {
    GMap<GUTF8String,GUTF8String> new_incl;
    const GUTF8String save_name(
      save_file(codebase,file,new_incl,get_data(load_name)));

    if(incl)
    {
      (*incl)[load_name]=save_name;
      for(GPosition pos=new_incl;pos;++pos)
      {
        save_file(codebase,file,incl);
      }
    }
  }
}

void
DjVmDoc::expand(const GURL &codebase, const GUTF8String &idx_name)
{
   DEBUG_MSG("DjVmDoc::expand(): Expanding into '" << codebase << "'\n");
   DEBUG_MAKE_INDENT(3);

   // Resolve any name conflicts
   // Find the list of all files.
   GPList<DjVmDir::File> files_list=dir->resolve_duplicates(false);

      // store each file
   for(GPosition pos=files_list;pos;++pos)
   {
     save_file(codebase,*files_list[pos]);
   }

   if (idx_name.length())
   {
      const GURL::UTF8 idx_url(idx_name, codebase);
   
      DEBUG_MSG("storing index file '" << idx_url << "'\n");

      DataPool::load_file(idx_url);
      GP<ByteStream> str=ByteStream::create(idx_url, "wb");
      write_index(str);
   }
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
