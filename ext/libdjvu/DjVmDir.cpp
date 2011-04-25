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

#include "DjVmDir.h"
#include "BSByteStream.h"
#include "GURL.h"
#include "debug.h"

#include <ctype.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


GP<DjVmDir::File>
DjVmDir::File::create(const GUTF8String &load_name,
  const GUTF8String &save_name, const GUTF8String &title,
  const FILE_TYPE file_type)
{
  File *file_ptr=new File();
  GP<File> file=file_ptr;
  file_ptr->set_load_name(load_name);
  file_ptr->set_save_name(save_name);
  file_ptr->set_title(title);
  file_ptr->flags=(file_type & TYPE_MASK);
  return file;
}

const GUTF8String &
DjVmDir::File::check_save_name(const bool xis_bundled)
{
  if(!xis_bundled && !valid_name)
  {
    GUTF8String retval=name.length()?name:id;
    if(GUTF8String(GNativeString(retval)) != retval)
    {
      const_cast<bool &>(valid_name)=true;
      char *buf;
      GPBuffer<char> gbuf(buf,2*retval.length()+1);
      char *s=buf;
      int i=0;
      for(char c=retval[i++];c;)
      {
        static const char hex[]="0123456789ABCDEF";
        int len=retval.nextChar(i)-i;
        if(len>1 || ((len == 1)&&(c&0x80)))
        {
          do
          {
            s++[0]=hex[(c>>4)&0xf];
            s++[0]=hex[(c&0xf)];
            c=retval[i++];
          } while(c && ((--len) > 0));
        }else
        {
          s++[0]=c;
          c=retval[i++];
        }
      }
      s++[0]=0;
      oldname=retval;
      name=buf;
    }
    const_cast<bool &>(valid_name)=true;
  }
  return *(name.length()?&name:&id);
}

const GUTF8String &
DjVmDir::File::get_save_name(void) const
{
  return *(name.length()?&name:&id);
}

void
DjVmDir::File::set_load_name(const GUTF8String &xid)
{
  GURL url=GURL::UTF8(xid);
  if(!url.is_valid())
  {
    url=GURL::Filename::UTF8(xid);
  }
  id=url.fname();
}

void
DjVmDir::File::set_save_name(const GUTF8String &xname)
{
  GURL url;
  valid_name=false;
  if(!xname.length())
  {
    GURL url=GURL::UTF8(id);
    if(!url.is_valid())
    {
      name=id;
    }else
    {
      name=url.fname();
    }
  }else
  {
    GURL url=GURL::UTF8(xname);
    if(!url.is_valid())
    {
      url=GURL::Filename::UTF8(xname);
    }
    name=url.fname();
  }
  oldname="";
}

/* DjVmDir::File */

DjVmDir::File::File(void) : offset(0), size(0), valid_name(false),
   flags(0), page_num(-1) { }

GUTF8String
DjVmDir::File::get_str_type(void) const
{
   GUTF8String type;
   switch(flags & TYPE_MASK)
   {
      case INCLUDE:
        type="INCLUDE";
        break;
      case PAGE:
        type="PAGE";
        break;
      case THUMBNAILS:
        type="THUMBNAILS";
        break;
      case SHARED_ANNO:
        type="SHARED_ANNO";
        break;
      default:
        //  Internal error: please modify DjVmDir::File::get_str_type()
        //  to contain all possible File types.
	      G_THROW( ERR_MSG("DjVmDir.get_str_type") );
   }
   return type;
}


const int DjVmDir::version=1;

void 
DjVmDir::decode(const GP<ByteStream> &gstr)
{
   ByteStream &str=*gstr;
   DEBUG_MSG("DjVmDir::decode(): decoding contents of 'DIRM' chunk...\n");
   DEBUG_MAKE_INDENT(3);
   
   GCriticalSectionLock lock(&class_lock);

   GPosition pos;

   files_list.empty();
   page2file.resize(-1);
   name2file.empty();
   id2file.empty();
   title2file.empty();

   int ver=str.read8();
   bool bundled=(ver & 0x80)!=0;
   ver&=0x7f;

   DEBUG_MSG("DIRM version=" << ver << ", our version=" << version << "\n");
   if (ver>version)
      G_THROW( ERR_MSG("DjVmDir.version_error") "\t" 
               + GUTF8String(version) + "\t" + GUTF8String(ver));
   // Unable to read DJVM directories of versions higher than xxx
   // Data version number is yyy.
   DEBUG_MSG("bundled directory=" << bundled << "\n");
   DEBUG_MSG("reading the directory records...\n");
   int files=str.read16();
   DEBUG_MSG("number of files=" << files << "\n");

   if (files)
   {
      DEBUG_MSG("reading offsets (and sizes for ver==0)\n");
      for(int nfile=0;nfile<files;nfile++)
      {
         GP<File> file=new File();
         files_list.append(file);
         if (bundled)
         {
            file->offset=str.read32();
            if (ver==0)
              file->size=str.read24();
            if (file->offset==0)
               G_THROW( ERR_MSG("DjVmDir.no_indirect") );
         } else
         {
           file->offset=file->size=0;
         }
      }

      GP<ByteStream> gbs_str=BSByteStream::create(gstr);
      ByteStream &bs_str=*gbs_str;
      if (ver>0)
      {
         DEBUG_MSG("reading and decompressing sizes...\n");
         for(GPosition pos=files_list;pos;++pos)
            files_list[pos]->size=bs_str.read24();
      }
         
      DEBUG_MSG("reading and decompressing flags...\n");
      for(pos=files_list;pos;++pos)
         files_list[pos]->flags=bs_str.read8();

      if (!ver)
      {
         DEBUG_MSG("converting flags from version 0...\n");
         for(pos=files_list;pos;++pos)
         {
            unsigned char flags_0=files_list[pos]->flags;
            unsigned char flags_1;
            flags_1=(flags_0 & File::IS_PAGE_0)?(File::PAGE):(File::INCLUDE);
            if (flags_0 & File::HAS_NAME_0)
              flags_1|=File::HAS_NAME;
            if (flags_0 & File::HAS_TITLE_0)
              flags_1|=File::HAS_TITLE;
            files_list[pos]->flags=flags_1;
         }
      }
   
      DEBUG_MSG("reading and decompressing names...\n");
      GTArray<char> strings;
      char buffer[1024];
      int length;
      while((length=bs_str.read(buffer, 1024)))
      {
         int strings_size=strings.size();
         strings.resize(strings_size+length-1);
         memcpy((char*) strings+strings_size, buffer, length);
      }
      DEBUG_MSG("size of decompressed names block=" << strings.size() << "\n");
   
         // Copy names into the files
      const char * ptr=strings;
      for(pos=files_list;pos;++pos)
      {
         GP<File> file=files_list[pos];

         file->id=ptr;
         ptr+=file->id.length()+1;
         if (file->flags & File::HAS_NAME)
         {
            file->name=ptr;
            ptr+=file->name.length()+1;
         } else
         {
            file->name=file->id;
         }
         if (file->flags & File::HAS_TITLE)
         {
            file->title=ptr;
       ptr+=file->title.length()+1;
         } else
       file->title=file->id;
   /* msr debug:  multipage file, file->title is null.  
         DEBUG_MSG(file->name << ", " << file->id << ", " << file->title << ", " <<
                   file->offset << ", " << file->size << ", " <<
                   file->is_page() << "\n"); */
      }

         // Check that there is only one file with SHARED_ANNO flag on
      int shared_anno_cnt=0;
      for(pos=files_list;pos;++pos)
      {
         if (files_list[pos]->is_shared_anno())
         {
            shared_anno_cnt++;
         }
      }
      if (shared_anno_cnt>1)
        G_THROW( ERR_MSG("DjVmDir.corrupt") );

         // Now generate page=>file array for direct access
      int pages=0;
      for(pos=files_list;pos;++pos)
              pages+=files_list[pos]->is_page() ? 1 : 0;
      DEBUG_MSG("got " << pages << " pages\n");
      page2file.resize(pages-1);
      int page_num=0;
      for(pos=files_list;pos;++pos)
      {
               GP<File> file=files_list[pos];
               if (file->is_page())
               {
                  page2file[page_num]=file;
                  file->page_num=page_num++;
               }
      }

         // Generate name2file map
      for(pos=files_list;pos;++pos)
      {
	       GP<File> file=files_list[pos];
	       if (name2file.contains(file->name))
	          G_THROW( ERR_MSG("DjVmDir.dupl_name") "\t" + file->name );
	       name2file[file->name]=file;
      }

         // Generate id2file map
      for(pos=files_list;pos;++pos)
      {
	       GP<File> file=files_list[pos];
	       if (id2file.contains(file->id))
	          G_THROW( ERR_MSG("DjVmDir.dupl_id") "\t" + file->id);
	       id2file[file->id]=file;
      }

         // Generate title2file map
      for(pos=files_list;pos;++pos)
      {
	       GP<File> file=files_list[pos];
	       if (file->title.length())
	       {
	          if (title2file.contains(file->title))
	             G_THROW( ERR_MSG("DjVmDir.dupl_title") "\t" + file->title);
	          title2file[file->title]=file;
	       }
      }
   }
}


void
DjVmDir::encode(const GP<ByteStream> &gstr, const bool do_rename) const
{
  bool bundled = true;
  GPosition pos = files_list;
  if (files_list.size() && !files_list[pos]->offset)
    bundled = false;
  for (pos=files_list; pos; ++pos)
    if ( !bundled !=  !files_list[pos]->offset)
      //  There directory contains both indirect and bundled records.
      G_THROW( ERR_MSG("DjVmDir.bad_dir") );
  // Do the real work
  encode(gstr, bundled, do_rename);
}

void
DjVmDir::encode(const GP<ByteStream> &gstr, const bool bundled, const bool do_rename) const
{
  ByteStream &str=*gstr;
  DEBUG_MSG("DjVmDir::encode(): encoding contents of the 'DIRM' chunk do_rename=" << do_rename << "\n");
  DEBUG_MAKE_INDENT(3);
   
  GCriticalSectionLock lock((GCriticalSection *) &class_lock);
  GPosition pos;

  DEBUG_MSG("encoding version number=" << version << ", bundled=" << bundled << "\n");
  str.write8(version | ((int) bundled<< 7));
   
  DEBUG_MSG("storing the number of records=" << files_list.size() << "\n");
  str.write16(files_list.size());

  if (files_list.size())
    {
      // Check that there is only one file with shared annotations
      int shared_anno_cnt=0;
      for (pos=files_list; pos; ++pos)
        if (files_list[pos]->is_shared_anno())
          shared_anno_cnt++;
      if (shared_anno_cnt>1)
        G_THROW( ERR_MSG("DjVmDir.multi_save") );
      
      if (bundled)
        {
          // We need to store offsets uncompressed. That's because when
          // we save a DjVmDoc, we first compress the DjVmDir with dummy
          // offsets and after computing the real offsets we rewrite the
          // DjVmDir, which should not change its size during this operation
          DEBUG_MSG("storing offsets for every record\n");
          for (pos=files_list; pos; ++pos)
            {
              GP<File> file=files_list[pos];
              if (!file->offset)
                // The directory contains record without offset
                G_THROW( ERR_MSG("DjVmDir.bad_dir") );
              str.write32(file->offset);
            }
        }

      GP<ByteStream> gbs_str=BSByteStream::create(gstr, 50);
      ByteStream &bs_str=*gbs_str;
      DEBUG_MSG("storing and compressing sizes for every record\n");
      for (pos=files_list; pos; ++pos)
        {
          const GP<File> file(files_list[pos]);
          bs_str.write24(file->size);
        }
      DEBUG_MSG("storing and compressing flags for every record\n");
      const bool xdo_rename=(do_rename||!bundled);
      for (pos=files_list;pos;++pos)
	{
	  const GP<File> file(files_list[pos]);
	  if (xdo_rename)
	    {
	      const GUTF8String new_id = file->name;
	      if (!new_id)
		{
		  if (!file->oldname.length() || file->oldname == new_id)
		    file->flags &= ~File::HAS_NAME;
		  else
		    file->flags |= File::HAS_NAME;
		}
	    }
	  else
	    {
	      if (!file->name.length() || file->name == file->id)
		file->flags &= ~File::HAS_NAME;
	      else
		file->flags |= File::HAS_NAME;
	    }
	  if (file->title.length() && (file->title!=file->id))
	    file->flags |= File::HAS_TITLE;
	  else
	    file->flags &= ~File::HAS_TITLE;

	  bs_str.write8(file->flags);
	}

     DEBUG_MSG("storing and compressing names...\n");
     for(pos=files_list;pos;++pos)
     {
         GP<File> file=files_list[pos];
         GUTF8String id;
         GUTF8String name;
         GUTF8String title;
         if (xdo_rename)
           {
             id = file->name;
             if (! id)
               id = file->id;
             if ((file->flags) & File::HAS_NAME)
               name = file->oldname;
           }
         else
           {
             id=file->id;
             if ((file->flags) & File::HAS_NAME)
               name = file->name;
           }
         if ((file->flags) & File::HAS_TITLE)
           title = file->title;
         DEBUG_MSG("rename=" <<xdo_rename<<" id='" << id << "' name='" << name << "' title='" << title << "'\n");
         bs_str.writestring(id);
         bs_str.write8(0);
         if (name.length())
           {
             bs_str.writestring(name);
             bs_str.write8(0);
           }
         if (title.length())
           {
             bs_str.writestring(title);
             bs_str.write8(0);
           }
     }
    }
  DEBUG_MSG("done\n");
}

GP<DjVmDir::File>
DjVmDir::page_to_file(int page_num) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   return (page_num<page2file.size())?page2file[page_num]:(GP<DjVmDir::File>(0));
}

GP<DjVmDir::File>
DjVmDir::name_to_file(const GUTF8String & name) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   GPosition pos;
   return (name2file.contains(name, pos))?name2file[pos]:(GP<DjVmDir::File>(0));
}

GP<DjVmDir::File>
DjVmDir::id_to_file(const GUTF8String &id) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   GPosition pos;
   return (id2file.contains(id, pos))?id2file[pos]:(GP<DjVmDir::File>(0));
}

GP<DjVmDir::File>
DjVmDir::title_to_file(const GUTF8String &title) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   GPosition pos;
   return (title2file.contains(title, pos))?title2file[pos]:(GP<DjVmDir::File>(0));
}

GP<DjVmDir::File>
DjVmDir::pos_to_file(int fileno, int *ppageno) const
{
  GCriticalSectionLock lock((GCriticalSection *) &class_lock);
  GPosition pos = files_list;
  int pageno = 0;
  while (pos && --fileno >= 0) {
    if (files_list[pos]->is_page())
      ++pageno;
    ++pos;
  }
  if (!pos)
    return 0;
  if (ppageno)
    *ppageno = pageno;
  return files_list[pos];
}

GPList<DjVmDir::File>
DjVmDir::get_files_list(void) const
{
  GCriticalSectionLock lock((GCriticalSection *) &class_lock);
  return files_list;
}

int
DjVmDir::get_files_num(void) const
{
  GCriticalSectionLock lock((GCriticalSection *) &class_lock);
  return files_list.size();
}

int
DjVmDir::get_pages_num(void) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   return page2file.size();
}

int
DjVmDir::get_file_pos(const File * f) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   int cnt;
   GPosition pos;
   for(pos=files_list, cnt=0;pos&&(files_list[pos]!=f);++pos, cnt++)
                   continue;
   return (pos)?cnt:(-1);
}

int
DjVmDir::get_page_pos(int page_num) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   
   GP<File> file=page_to_file(page_num);
   return (file)?get_file_pos(file):(-1);
}

GP<DjVmDir::File>
DjVmDir::get_shared_anno_file(void) const
{
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   GP<File> file;
   for(GPosition pos=files_list;pos;++pos)
   {
      GP<File> frec=files_list[pos];
      if (frec->is_shared_anno())
      {
         file=frec;
         break;
      }
   }
   return file;
}

int
DjVmDir::insert_file(const GP<File> & file, int pos_num)
{
   DEBUG_MSG("DjVmDir::insert_file(): name='" 
             << file->name << "', pos=" << pos_num << "\n");
   DEBUG_MAKE_INDENT(3);
   
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   
   if (pos_num<0)
     pos_num=files_list.size();

   //// Modify maps
   //   if (! File::is_legal_id(file->id))
   //     G_THROW( ERR_MSG("DjVmDir.bad_file") "\t" + file->id);
   if (id2file.contains(file->id))
     G_THROW( ERR_MSG("DjVmDir.dupl_id2") "\t" + file->id);
   if (name2file.contains(file->name))
     G_THROW( ERR_MSG("DjVmDir.dupl_name2") "\t" + file->name);
   name2file[file->name]=file;
   id2file[file->id]=file;
   if (file->title.length())
     {
       if (title2file.contains(file->title))  
         // duplicate titles may become ok some day
         G_THROW( ERR_MSG("DjVmDir.dupl_title2") "\t" + file->title);
       title2file[file->title]=file;
     }

      // Make sure that there is no more than one file with shared annotations
   if (file->is_shared_anno())
   {
      for(GPosition pos=files_list;pos;++pos)
         if (files_list[pos]->is_shared_anno())
            G_THROW( ERR_MSG("DjVmDir.multi_save2") );
   }
   
      // Add the file to the list
   int cnt;
   GPosition pos;
   for(pos=files_list, cnt=0;pos&&(cnt!=pos_num);++pos, cnt++)
                   continue;
   if (pos)
     files_list.insert_before(pos, file);
   else
     files_list.append(file);

   if (file->is_page())
   {
         // This file is also a page
         // Count its number
      int page_num=0;
      for(pos=files_list;pos;++pos)
      {
         GP<File> &f=files_list[pos];
         if (f==file)
           break;
         if (f->is_page())
           page_num++;
      }

      int i;
      page2file.resize(page2file.size());
      for(i=page2file.size()-1;i>page_num;i--)
         page2file[i]=page2file[i-1];
      page2file[page_num]=file;
      for(i=page_num;i<page2file.size();i++)
         page2file[i]->page_num=i;
   }
   return pos_num;
}

void
DjVmDir::delete_file(const GUTF8String &id)
{
   DEBUG_MSG("Deleting file with id='" << (const char *)id << "'\n");
   DEBUG_MAKE_INDENT(3);

   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   
   for(GPosition pos=files_list;pos;++pos)
   {
      GP<File> & f=files_list[pos];
      if (id == f->id)
      {
         name2file.del(f->name);
         id2file.del(f->id);
         title2file.del(f->title);
         if (f->is_page())
         {
            for(int page=0;page<page2file.size();page++)
            {
               if (page2file[page]==f)
               {
                  int i;
                  for(i=page;i<page2file.size()-1;i++)
                     page2file[i]=page2file[i+1];
                  page2file.resize(page2file.size()-2);
                  for(i=page;i<page2file.size();i++)
                     page2file[i]->page_num=i;
                  break;
               }
            }
         }
         files_list.del(pos);
         break;
      }
   }
}

void
DjVmDir::set_file_name(const GUTF8String &id, const GUTF8String &name)
{
   DEBUG_MSG("DjVmDir::set_file_name(): id='" << id << "', name='" << name << "'\n");
   DEBUG_MAKE_INDENT(3);
   
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   GPosition pos;
   
      // First see, if the name is unique
   for(pos=files_list;pos;++pos)
   {
      GP<File> file=files_list[pos];
      if (file->id!=id && file->name==name)
        G_THROW( ERR_MSG("DjVmDir.name_in_use") "\t" + GUTF8String(name));
   }

      // Check if ID is valid
   if (!id2file.contains(id, pos))
      G_THROW( ERR_MSG("DjVmDir.no_info") "\t" + GUTF8String(id));
   GP<File> file=id2file[pos];
   name2file.del(file->name);
   file->name=name;
   name2file[name]=file;
}

void
DjVmDir::set_file_title(const GUTF8String &id, const GUTF8String &title)
{
   DEBUG_MSG("DjVmDir::set_file_title(): id='" << id << "', title='" << title << "'\n");
   DEBUG_MAKE_INDENT(3);
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   GPosition pos;
      // Check if ID is valid
   if (!id2file.contains(id, pos))
      G_THROW( ERR_MSG("DjVmDir.no_info") "\t" + GUTF8String(id));
   GP<File> file=id2file[pos];
   title2file.del(file->title);
   file->title=title;
   title2file[title]=file;
}

GPList<DjVmDir::File>
DjVmDir::resolve_duplicates(const bool save_as_bundled)
{
  GCriticalSectionLock lock((GCriticalSection *) &class_lock);
  // Make sure all the filenames are unique.
  GPosition pos;
  GMap<GUTF8String,void *> save_map;
  GMap<GUTF8String,GPList<DjVmDir::File> > conflicts;
  for(pos=files_list;pos;++pos)
  {
    const GUTF8String save_name=files_list[pos]->check_save_name(save_as_bundled).downcase();
    if(save_map.contains(save_name))
    {
      conflicts[save_name].append(files_list[pos]);
    }else
    {
      save_map[save_name]=0;
    }
  }
  for(pos=conflicts;pos;++pos)
  {
    const GUTF8String &save_name=conflicts.key(pos);
    const int dot=save_name.rsearch('.',0);
    GPList<DjVmDir::File> &cfiles=conflicts[pos];
    int count=1;
    for(GPosition qpos=cfiles;qpos;++qpos)
    {
      GUTF8String new_name=cfiles[qpos]->get_load_name();
      if((new_name != GUTF8String(GNativeString(new_name)))
        ||conflicts.contains(new_name))
      {
        do
        {
          new_name=(dot<0)
            ?(save_name+"-"+GUTF8String(count++))
            :(save_name.substr(0,dot)+"-"+GUTF8String(count++)+
              save_name.substr(dot,(unsigned int)(-1)));
        } while(save_map.contains(new_name.downcase()));
      }
      cfiles[qpos]->set_save_name(new_name);
      save_map[new_name]=0;
    }
  }
  return files_list;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
