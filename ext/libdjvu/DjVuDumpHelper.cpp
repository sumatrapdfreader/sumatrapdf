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

#include "DjVuDumpHelper.h"
#include "DataPool.h"
#include "DjVmDir.h"
#include "DjVuInfo.h"
#include "IFFByteStream.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


#ifdef putchar
#undef putchar
#endif

struct DjVmInfo
{
  GP<DjVmDir> dir;
  GPMap<int,DjVmDir::File> map;
};

inline static void
putchar(ByteStream & str, char ch)
{
   str.write(&ch, 1);
}

// ---------- ROUTINES FOR SUMMARIZING CHUNK DATA

static void
display_djvu_info(ByteStream & out_str, IFFByteStream &iff,
		  GUTF8String, size_t size, DjVmInfo&, int)
{
  GP<DjVuInfo> ginfo=DjVuInfo::create();
  DjVuInfo &info=*ginfo;
  info.decode(*iff.get_bytestream());
  if (size >= 4)
    out_str.format( "DjVu %dx%d", info.width, info.height);
  if (size >= 5)
    out_str.format( ", v%d", info.version);
  if (size >= 8)
    out_str.format( ", %d dpi", info.dpi);
  if (size >= 9)
    out_str.format( ", gamma=%3.1f", info.gamma);
}

static void
display_djbz(ByteStream & out_str, IFFByteStream &iff,
	     GUTF8String, size_t, DjVmInfo&, int)
{
  out_str.format( "JB2 shared dictionary");
}

static void
display_fgbz(ByteStream & out_str, IFFByteStream &iff,
	     GUTF8String, size_t, DjVmInfo&, int)
{
  GP<ByteStream> gbs = iff.get_bytestream();
  int version = gbs->read8();
  int size = gbs->read16();
  out_str.format( "JB2 colors data, v%d, %d colors", 
                  version & 0x7f, size);
}

static void
display_sjbz(ByteStream & out_str, IFFByteStream &iff,
	     GUTF8String, size_t, DjVmInfo&, int)
{
  out_str.format( "JB2 bilevel data");
}

static void
display_smmr(ByteStream & out_str, IFFByteStream &iff,
	     GUTF8String, size_t, DjVmInfo&, int)
{
  out_str.format( "G4/MMR stencil data");
}

static void
display_iw4(ByteStream & out_str, IFFByteStream &iff,
	    GUTF8String, size_t, DjVmInfo&, int)
{
  GP<ByteStream> gbs = iff.get_bytestream();
  unsigned char serial = gbs->read8();
  unsigned char slices = gbs->read8();
  out_str.format( "IW4 data #%d, %d slices", serial+1, slices);
  if (serial == 0)
    {
      unsigned char major = gbs->read8();
      unsigned char minor = gbs->read8();
      unsigned char xhi = gbs->read8();
      unsigned char xlo = gbs->read8();
      unsigned char yhi = gbs->read8();
      unsigned char ylo = gbs->read8();
      out_str.format( ", v%d.%d (%s), %dx%d", major & 0x7f, minor,
                      (major & 0x80 ? "b&w" : "color"), 
                      (xhi<<8)+xlo, (yhi<<8)+ylo );
    }
}

static void
display_djvm_dirm(ByteStream & out_str, IFFByteStream & iff,
		  GUTF8String head, size_t, DjVmInfo& djvminfo, int)
{
  GP<DjVmDir> dir = DjVmDir::create();
  dir->decode(iff.get_bytestream());
  GPList<DjVmDir::File> list = dir->get_files_list();
  if (dir->is_indirect())
  {
    out_str.format( "Document directory (indirect, %d files %d pages)", 
	                  dir->get_files_num(), dir->get_pages_num());
    for (GPosition p=list; p; ++p)
      out_str.format( "\n%s%s -> %s", (const char*)head, 
                      (const char*)list[p]->get_load_name(), (const char*)list[p]->get_save_name() );
  }
  else
  {
    out_str.format( "Document directory (bundled, %d files %d pages)", 
	                  dir->get_files_num(), dir->get_pages_num());
    djvminfo.dir = dir;
    djvminfo.map.empty();
    for (GPosition p=list; p; ++p)
      djvminfo.map[list[p]->offset] = list[p];
  }
}

static void
display_th44(ByteStream & out_str, IFFByteStream & iff,
	     GUTF8String, size_t, DjVmInfo & djvminfo, int counter)
{
   int start_page=-1;
   if (djvminfo.dir)
   {
      GPList<DjVmDir::File> files_list=djvminfo.dir->get_files_list();
      for(GPosition pos=files_list;pos;++pos)
      {
	 GP<DjVmDir::File> frec=files_list[pos];
	 if (iff.tell()>=frec->offset &&
	     iff.tell()<frec->offset+frec->size)
	 {
	    while(pos && !files_list[pos]->is_page())
	       ++pos;
	    if (pos)
	       start_page=files_list[pos]->get_page_num();
	    break;
	 }
      }
   }
   if (start_page>=0)
      out_str.format( "Thumbnail icon for page %d", start_page+counter+1);
   else
      out_str.format( "Thumbnail icon");
}

static void
display_incl(ByteStream & out_str, IFFByteStream & iff,
	     GUTF8String, size_t, DjVmInfo&, int)
{
   GUTF8String name;
   char ch;
   while(iff.read(&ch, 1) && ch!='\n')
     name += ch;
   out_str.format( "Indirection chunk --> {%s}", (const char *) name);
}

static void
display_anno(ByteStream & out_str, IFFByteStream &iff,
	     GUTF8String, size_t, DjVmInfo&, int)
{
   out_str.format( "Page annotation");
   GUTF8String id;
   iff.short_id(id);
   out_str.format( " (hyperlinks, etc.)");
}

static void
display_text(ByteStream & out_str, IFFByteStream &iff,
	     GUTF8String, size_t, DjVmInfo&, int)
{
   out_str.format( "Hidden text");
   GUTF8String id;
   iff.short_id(id);
   out_str.format( " (text, etc.)");
}

struct displaysubr
{
  const char *id;
  void (*subr)(ByteStream &, IFFByteStream &, GUTF8String,
	       size_t, DjVmInfo&, int counter);
};
 
static displaysubr disproutines[] = 
{
  { "DJVU.INFO", display_djvu_info },
  { "DJVU.Smmr", display_smmr },
  { "DJVU.Sjbz", display_sjbz },
  { "DJVU.Djbz", display_djbz },
  { "DJVU.FG44", display_iw4 },
  { "DJVU.BG44", display_iw4 },
  { "DJVU.FGbz", display_fgbz },
  { "DJVI.Sjbz", display_sjbz },
  { "DJVI.Djbz", display_djbz },
  { "DJVI.FGbz", display_fgbz },
  { "DJVI.FG44", display_iw4 },
  { "DJVI.BG44", display_iw4 },
  { "BM44.BM44", display_iw4 },
  { "PM44.PM44", display_iw4 },
  { "DJVM.DIRM", display_djvm_dirm },
  { "THUM.TH44", display_th44 },
  { "INCL", display_incl },
  { "ANTa", display_anno },
  { "ANTz", display_anno },
  { "TXTa", display_text },
  { "TXTz", display_text },
  { 0, 0 },
};

// ---------- ROUTINES FOR DISPLAYING CHUNK STRUCTURE

static void
display_chunks(ByteStream & out_str, IFFByteStream &iff,
	       const GUTF8String &head, DjVmInfo djvminfo)
{
  size_t size;
  GUTF8String id, fullid;
  GUTF8String head2 = head + "  ";
  GPMap<int,DjVmDir::File> djvmmap;
  int rawoffset;
  GMap<GUTF8String, int> counters;
  
  while ((size = iff.get_chunk(id, &rawoffset)))
  {
    if (!counters.contains(id)) counters[id]=0;
    else counters[id]++;
    
    GUTF8String msg;
    msg.format("%s%s [%d] ", (const char *)head, (const char *)id, size);
    out_str.format( "%s", (const char *)msg);
    // Display DJVM is when adequate
    if (djvminfo.dir)
    {
      GP<DjVmDir::File> rec = djvminfo.map[rawoffset];
      if (rec)
        {
          GUTF8String id = rec->get_load_name();
          GUTF8String title = rec->get_title();
          out_str.format( "{%s}", (const char*) id);
          if (rec->is_include())
            out_str.format(" [I]");
          if (rec->is_thumbnails())
            out_str.format(" [T]");
          if (rec->is_shared_anno())
            out_str.format(" [S]");
          if (rec->is_page())
            out_str.format(" [P%d]", rec->get_page_num()+1);
          if (id != title)
            out_str.format(" (%s)", (const char*)title);
        }
    }
    // Test chunk type
    iff.full_id(fullid);
    for (int i=0; disproutines[i].id; i++)
      if (fullid == disproutines[i].id || id == disproutines[i].id)
      {
        int n = msg.length();
        while (n++ < 14+(int) head.length()) putchar(out_str, ' ');
        if (!iff.composite()) out_str.format( "    ");
        (*disproutines[i].subr)(out_str, iff, head2,
                                size, djvminfo, counters[id]);
        break;
      }
      // Default display of composite chunk
      out_str.format( "\n");
      if (iff.composite())
        display_chunks(out_str, iff, head2, djvminfo);
      // Terminate
      iff.close_chunk();
  }
}

GP<ByteStream>
DjVuDumpHelper::dump(const GP<DataPool> & pool)
{
   return dump(pool->get_stream());
}

GP<ByteStream>
DjVuDumpHelper::dump(GP<ByteStream> gstr)
{
   GP<ByteStream> out_str=ByteStream::create();
   GUTF8String head="  ";
   GP<IFFByteStream> iff=IFFByteStream::create(gstr);
   DjVmInfo djvminfo;
   display_chunks(*out_str, *iff, head, djvminfo);
   return out_str;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
