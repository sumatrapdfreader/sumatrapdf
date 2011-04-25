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

#include "GIFFManager.h"
#include "GException.h"
#include "debug.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


GIFFChunk::~GIFFChunk(void) {}

GIFFManager::~GIFFManager(void) {}

GP<GIFFManager> 
GIFFManager::create(void)
{
  GIFFManager *iff=new GIFFManager();
  GP<GIFFManager> retval=iff;
  iff->init();
  return retval;
}

GP<GIFFManager> 
GIFFManager::create(const GUTF8String &name)
{
  GIFFManager *iff=new GIFFManager();
  GP<GIFFManager> retval=iff;
  iff->init(name);
  return retval;
}

void
GIFFChunk::set_name(GUTF8String name)
{
  DEBUG_MSG("GIFFChunk::set_name(): name='" << name << "'\n");
  DEBUG_MAKE_INDENT(3);

  const int colon=name.search(':');
  if(colon>=0)
  {
    type=name.substr(0,colon);
    name=name.substr(colon+1,(unsigned int)-1);
    if(name.search(':')>=0)
      G_THROW( ERR_MSG("GIFFManager.one_colon") );
  }

  DEBUG_MSG("auto-setting type to '" << type << "'\n");

  if (name.contains(".[]")>=0)
    G_THROW( ERR_MSG("GIFFManager.bad_char") );

  strncpy(GIFFChunk::name, (const char *)name, 4);
  GIFFChunk::name[4]=0;
  for(int i=strlen(GIFFChunk::name);i<4;i++)
    GIFFChunk::name[i]=' ';
}

bool
GIFFChunk::check_name(GUTF8String name)
{
  GUTF8String type;
  const int colon=name.search(':');
  if(colon>=0)
    {
      type=name.substr(0,colon);
      name=name.substr(colon+1,(unsigned int)-1);
    }

  const GUTF8String sname=(name.substr(0,4)+"    ").substr(0,4);

  DEBUG_MSG("GIFFChunk::check_name(): type='" << type << "' name='" << sname << "'\n");
  return (type==GIFFChunk::type || (!type.length() && GIFFChunk::type=="FORM"))
    && sname==GIFFChunk::name;
}

void
GIFFChunk::save(IFFByteStream & istr, bool use_trick)
{
  DEBUG_MSG("GIFFChunk::save(): saving chunk '" << get_full_name() << "'\n");
  DEBUG_MAKE_INDENT(3);

  if (is_container())
  {
    istr.put_chunk(get_full_name(), use_trick);
    if (chunks.size())
    {
      GPosition pos;
      for(pos=chunks;pos;++pos)
        if (chunks[pos]->get_type()=="PROP")
          chunks[pos]->save(istr);
      for(pos=chunks;pos;++pos)
        if (chunks[pos]->get_type()!="PROP")
          chunks[pos]->save(istr);
    } else
    {
      DEBUG_MSG("but it's empty => saving empty container.\n");
    }
    istr.close_chunk();
  } else
  {
    istr.put_chunk(get_name(), use_trick);
    istr.get_bytestream()->writall((const char *) data, data.size());
    istr.close_chunk();
  }
}

void
GIFFChunk::add_chunk(const GP<GIFFChunk> & chunk, int position)
{
  DEBUG_MSG("GIFFChunk::add_chunk(): Adding chunk to '" << get_name() <<
     "' @ position=" << position << "\n");
  DEBUG_MAKE_INDENT(3);

  if (!type.length())
  {
    DEBUG_MSG("Converting the parent to FORM\n");
    type="FORM";
  }

  if (chunk->get_type()=="PROP")
  {
    DEBUG_MSG("Converting the parent to LIST\n");
    type="LIST";
  }

  GPosition pos;
  if (position>=0 && chunks.nth(position, pos))
  {
    chunks.insert_before(pos, chunk);
  }else
  {
    chunks.append(chunk);
  }
}

GUTF8String 
GIFFChunk::decode_name(const GUTF8String &name, int &number)
{
  DEBUG_MSG("GIFFChunk::decode_name(): Checking brackets in name '" << name << "'\n");
  DEBUG_MAKE_INDENT(3);
   
  if (name.search('.')>=0)
    G_THROW( ERR_MSG("GIFFManager.no_dots") );

  number=0;
  const int obracket=name.search('[');
  GUTF8String short_name;
  if (obracket >= 0)
  {
    const int cbracket=name.search(']',obracket+1);
    if (cbracket < 0)
      G_THROW( ERR_MSG("GIFFManager.unmatched") );
    if (name.length() > (unsigned int)(cbracket+1))
      G_THROW( ERR_MSG("GIFFManager.garbage") );
//    number =atoi((const char *)name.substr(obracket+1,cbracket-obracket-1));
    number= name.substr(obracket+1,cbracket-obracket-1).toInt(); 
    short_name=name.substr(0,obracket);
  }else
  {
    short_name=name;
  }

  const int colon=short_name.search(':');
  if (colon>=0)
    short_name=short_name.substr(colon+1,(unsigned int)-1);

  for(int i=short_name.length();i<4;i++)
    short_name.setat(i, ' ');
   
  DEBUG_MSG("short_name='" << short_name << "'\n");
  DEBUG_MSG("number=" << number << "\n");
   
  return short_name;
}

void
GIFFChunk::del_chunk(const GUTF8String &name)
   // The name may contain brackets to specify the chunk number
{
  DEBUG_MSG("GIFFChunk::del_chunk(): Deleting chunk '" << name <<
     "' from '" << get_name() << "'\n");
  DEBUG_MAKE_INDENT(3);

  int number;
  const GUTF8String short_name=decode_name(name,number);

  GPosition pos=chunks;
  for(int num=0;pos;++pos)
  {
    if ((chunks[pos]->get_name()==short_name)&&(num++ == number))
    {
      chunks.del(pos);
      break;
    }
  }
  if(! pos)
  {
    G_THROW( ERR_MSG("GIFFManager.no_chunk") "\t"+short_name+"\t"+GUTF8String(number)+"\t"+get_name());
  }
}

GP<GIFFChunk>
GIFFChunk::get_chunk(const GUTF8String &name, int * pos_ptr)
   // The name may contain brackets to specify the chunk number
{
  DEBUG_MSG("GIFFChunk::get_chunk(): Returning chunk '" << name <<
     "' from '" << get_name() << "'\n");
  DEBUG_MAKE_INDENT(3);

  int number;
  const GUTF8String short_name=decode_name(name,number);

  int num=0;
  int pos_num;
  GP<GIFFChunk> retval;
  GPosition pos;
  for(pos=chunks, pos_num=0;pos;++pos, pos_num++)
  {
    if (chunks[pos]->get_name()==short_name && num++==number)
    {
      if (pos_ptr)
        *pos_ptr=pos_num;
      retval=chunks[pos];
      break;
    }
  }
  return retval;
}

int
GIFFChunk::get_chunks_number(void)
{
  DEBUG_MSG("GIFFChunk::get_chunks_number(): Returning number of chunks '" << name <<
     "' in '" << get_name() << "'\n");
  DEBUG_MAKE_INDENT(3);
  return chunks.size();
}

int
GIFFChunk::get_chunks_number(const GUTF8String &name)
{
  DEBUG_MSG("GIFFChunk::get_chunks_number(): Returning number of chunks '" << name <<
     "' in '" << get_name() << "'\n");
  DEBUG_MAKE_INDENT(3);

  if (name.contains("[]")>=0)
    G_THROW( ERR_MSG("GIFFManager.no_brackets") );
  
  int number; 
  GUTF8String short_name=decode_name(name,number);
   
  int num=0;
  for(GPosition pos=chunks;pos;++pos)
     num+=(chunks[pos]->get_name()==short_name);
  return num;
}

//************************************************************************

void
GIFFManager::add_chunk(GUTF8String parent_name, const GP<GIFFChunk> & chunk,
		       int pos)
      // parent_name is the fully qualified name of the PARENT
      //             IT MAY BE EMPTY
      // All the required chunks will be created
      // pos=-1 means to append the chunk
{
  DEBUG_MSG("GIFFManager::add_chunk(): Adding chunk to name='" << parent_name << "'\n");
  DEBUG_MAKE_INDENT(3);
   
  if (!top_level->get_name().length())
  {
    if ((!parent_name.length())||(parent_name[0]!='.'))
      G_THROW( ERR_MSG("GIFFManager.no_top_name") );
    if (parent_name.length() < 2)
    {
      // 'chunk' is actually the new top-level chunk
      DEBUG_MSG("since parent_name=='.', making the chunk top-level\n");
      if (!chunk->is_container())
        G_THROW( ERR_MSG("GIFFManager.no_top_cont") );
      top_level=chunk;
      return;
    }

    DEBUG_MSG("Setting the name of the top-level chunk\n");
    const int next_dot=parent_name.search('.',1);
    if(next_dot>=0)
    {
      top_level->set_name(parent_name.substr(1,next_dot-1));
    }else
    {
      top_level->set_name(parent_name.substr(1,(unsigned int)-1));
    }
  }

  DEBUG_MSG("top level chunk name='" << top_level->get_name() << "'\n");
   
  if (parent_name.length() && parent_name[0] == '.')
  {
    int next_dot=parent_name.search('.',1);
    if(next_dot<0)
    {
      next_dot=parent_name.length();
    }
    GUTF8String top_name=parent_name.substr(1,next_dot-1);
    if (!top_level->check_name(top_name))
      G_THROW( ERR_MSG("GIFFManager.wrong_name") "\t"+top_name);
    parent_name=parent_name.substr(next_dot,(unsigned int)-1);
  }

  GP<GIFFChunk> cur_sec=top_level;
  const char * start, * end=(const char *)parent_name-1;
  do
  {
    for(start=++end;*end&&(*end!='.');end++)
      EMPTY_LOOP;
    if (end>start)
    {
      GUTF8String name(start,end-start);
      GUTF8String short_name;
      int number=0;
      const int obracket=name.search('[');
      if (obracket >= 0)
      {
        const int cbracket=name.search(']',obracket+1);
        if (cbracket < 0)
          G_THROW( ERR_MSG("GIFFManager.unmatched") );
//        number=atoi((const char *)name.substr(obracket+1,cbracket-obracket-1));
        number = name.substr(obracket+1,cbracket-obracket-1).toInt();
        short_name=name.substr(0,obracket);
      }else
      {
        short_name=name;
      }

      for(int i=cur_sec->get_chunks_number(short_name);i<number+1;i++)
        cur_sec->add_chunk(GIFFChunk::create(short_name));
      cur_sec=cur_sec->get_chunk(name);
      if (!cur_sec)
        G_THROW( ERR_MSG("GIFFManager.unknown") "\t"+name);
    }
  } while(*end);
  cur_sec->add_chunk(chunk, pos);
}

void
GIFFManager::add_chunk(GUTF8String name, const TArray<char> & data)
      // name is fully qualified name of the chunk TO BE INSERTED.
      //      it may contain brackets at the end to set the position
      // All the required chunks will be created
{
  DEBUG_MSG("GIFFManager::add_chunk(): adding plain chunk with name='" << name << "'\n");
  DEBUG_MAKE_INDENT(3);

  GUTF8String chunk_name;
  const int lastdot=name.rsearch('.');
  if(lastdot < 0)
  {
    chunk_name=name;
    name=name.substr(0,lastdot);
  }else
  {
    chunk_name=name.substr(lastdot+1,(unsigned int)-1);
  }

  int pos=-1;
  const int obracket=chunk_name.search('[');
  if (obracket >= 0)
  {
    const int cbracket=chunk_name.search(']',obracket+1);
    if (cbracket < 0)
      G_THROW( ERR_MSG("GIFFManager.unmatched") );
    if (name.length() > (unsigned int)(cbracket+1))
      G_THROW( ERR_MSG("GIFFManager.garbage") );
//    pos=atoi((const char *)chunk_name.substr(obracket+1,cbracket-obracket-1));
    pos = chunk_name.substr(obracket+1,cbracket-obracket-1).toInt();
    chunk_name=chunk_name.substr(0,obracket);
  }
  DEBUG_MSG("Creating new chunk with name " << chunk_name << "\n");
  GP<GIFFChunk> chunk;
  chunk=GIFFChunk::create(chunk_name, data);
  add_chunk(name, chunk, pos);
}

void
GIFFManager::del_chunk(void)
{
  DEBUG_MSG("GIFFManager::del_chunk(): Deleting chunk\n");
  DEBUG_MAKE_INDENT(3);
   
  G_THROW( ERR_MSG("GIFFManager.del_empty") );
}

void
GIFFManager::del_chunk(GUTF8String name)
      // "name" should be fully qualified, that is contain dots.
      // It may also end with [] to set the chunk order number
{
  DEBUG_MSG("GIFFManager::del_chunk(): Deleting chunk '" << name << "'\n");
  DEBUG_MAKE_INDENT(3);
   
  if (!name.length())
    G_THROW( ERR_MSG("GIFFManager.del_empty") );

  if (name[0]=='.')
  {
    const int next_dot=name.search('.',1);
    if (next_dot < 0)
    {
      if (top_level->check_name(name.substr(1,(unsigned int)-1)))
      {
        DEBUG_MSG("Removing top level chunk..\n");
        top_level=GIFFChunk::create();
        return;
      }
      G_THROW( ERR_MSG("GIFFManager.wrong_name2") "\t"+name.substr(1,(unsigned int)-1));
    }
    const GUTF8String top_name=name.substr(1,next_dot-1);
    if (!top_level->check_name(top_name))
      G_THROW( ERR_MSG("GIFFManager.wrong_name2") "\t"+top_name);
    name=name.substr(next_dot+1,(unsigned int)-1);
  }
   
  GP<GIFFChunk> cur_sec=top_level;
  const char * start, * end=(const char *)name-1;
  do
  {
    for(start=++end;*end&&(*end!='.');end++)
      EMPTY_LOOP;
    if (end>start && *end=='.')
      cur_sec=cur_sec->get_chunk(GUTF8String(start, end-start));
    if (!cur_sec)
      G_THROW( ERR_MSG("GIFFManager.cant_find") "\t"+GUTF8String(name));
  } while(*end);
   
  if (!start[0])
  {
    G_THROW(GUTF8String( ERR_MSG("GIFFManager.malformed") "\t")+name);
  }
   
  cur_sec->del_chunk(start);
}

GP<GIFFChunk>
GIFFManager::get_chunk(GUTF8String name, int * pos_num)
      // "name" should be fully qualified, that is contain dots.
      // It may also end with [] to set the chunk order number
{
  DEBUG_MSG("GIFFManager::get_chunk(): Returning chunk '" << name << "'\n");
  DEBUG_MAKE_INDENT(3);
   
  if (!name.length())
    G_THROW( ERR_MSG("GIFFManager.get_empty") );

  if (name[0]=='.')
  {
    const int next_dot=name.search('.',1);
    if (next_dot < 0)
    {
      if (top_level->check_name(name.substr(1,(unsigned int)-1)))
      {
        DEBUG_MSG("Removing top level chunk..\n");
        return top_level;
      }
      G_THROW( ERR_MSG("GIFFManager.wrong_name2") "\t"+name.substr(1,(unsigned int)-1));
    }
    const GUTF8String top_name=name.substr(1,next_dot-1);
    if (!top_level->check_name(top_name))
      G_THROW( ERR_MSG("GIFFManager.wrong_name2") "\t"+top_name);
    name=name.substr(next_dot+1,(unsigned int)-1);
  }
   
  GP<GIFFChunk> cur_sec=top_level;
  const char * start, * end=(const char *) name-1;
  do
  {
    for(start=++end;*end&&(*end!='.');end++)
      EMPTY_LOOP;
    if (end>start)
      cur_sec=cur_sec->get_chunk(GUTF8String(start, end-start), pos_num);
    if (!cur_sec)
      break;
  } while(*end);
   
  return cur_sec;
}

int
GIFFManager::get_chunks_number(void)
{
  DEBUG_MSG("GIFFManager::get_chunks_number()\n");
  DEBUG_MAKE_INDENT(3);
  return top_level->get_chunks_number();
}

int
GIFFManager::get_chunks_number(const GUTF8String &name)
   // Returns the number of chunks with given fully qualified name
{
  DEBUG_MSG("GIFFManager::get_chunks_number(): name='" << name << "'\n");
  DEBUG_MAKE_INDENT(3);

  int retval;
  const int last_dot=name.rsearch('.');
  if (last_dot<0)
  {
    retval=top_level->get_chunks_number(name);
  }else if(!last_dot)
  {
    retval=(top_level->get_name()==name.substr(1,(unsigned int)-1))?1:0;
  }else
  {
    GP<GIFFChunk> chunk=get_chunk(name.substr(0,last_dot));
    retval=( chunk
      ?(chunk->get_chunks_number(name.substr(last_dot+1,(unsigned int)-1)))
      :0 );
  }
  return retval;
}

void
GIFFManager::load_chunk(IFFByteStream & istr, GP<GIFFChunk> chunk)
{
  DEBUG_MSG("GIFFManager::load_chunk(): loading contents of chunk '" <<
    chunk->get_name() << "'\n");
  DEBUG_MAKE_INDENT(3);
   
  int chunk_size;
  GUTF8String chunk_id;
  while ((chunk_size=istr.get_chunk(chunk_id)))
  {
    if (istr.check_id(chunk_id))
    {
      GP<GIFFChunk> ch=GIFFChunk::create(chunk_id);
      load_chunk(istr, ch);
      chunk->add_chunk(ch);
    } else
    {
      TArray<char> data(chunk_size-1);
      istr.get_bytestream()->readall( (char*)data, data.size());
      GP<GIFFChunk> ch=GIFFChunk::create(chunk_id, data);
      chunk->add_chunk(ch);
    }
    istr.close_chunk();
  }
}

void
GIFFManager::load_file(const TArray<char> & data)
{
  GP<ByteStream> str=ByteStream::create((const char *)data, data.size());
  load_file(str);
}

void
GIFFManager::load_file(GP<ByteStream> str)
{
  DEBUG_MSG("GIFFManager::load_file(): Loading IFF file.\n");
  DEBUG_MAKE_INDENT(3);
   
  GP<IFFByteStream> gistr=IFFByteStream::create(str);
  IFFByteStream &istr=*gistr;
  GUTF8String chunk_id;
  if (istr.get_chunk(chunk_id))
  {
    if (chunk_id.substr(0,5) != "FORM:")
      G_THROW( ERR_MSG("GIFFManager.cant_find2") );
    set_name(chunk_id);
    load_chunk(istr, top_level);
    istr.close_chunk();
  }
}

void
GIFFManager::save_file(TArray<char> & data)
{
  GP<ByteStream> gstr=ByteStream::create();
  save_file(gstr);
  data=gstr->get_data();
}

void
GIFFManager::save_file(GP<ByteStream> str)
{
  GP<IFFByteStream> istr=IFFByteStream::create(str);
  top_level->save(*istr, 1);
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

