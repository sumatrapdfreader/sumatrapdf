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

#include "DjVmDir0.h"
#include "ByteStream.h"
#include "debug.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

int
DjVmDir0::get_size(void) const
      // WARNING! make sure, that get_size(), encode() and decode() are in sync
{
   int size=0;

   size+=2;	// number of files
   for(int i=0;i<num2file.size();i++)
   {
      FileRec & file=*num2file[i];
      size+=file.name.length()+1;	// file name
      size+=1;				// is IFF file
      size+=4;				// file offset
      size+=4;				// file size
   };

   return size;
}

#ifndef NEED_DECODER_ONLY
void
DjVmDir0::encode(ByteStream & bs)
      // WARNING! make sure, that get_size(), encode() and decode() are in sync
{
   bs.write16(num2file.size());
   for(int i=0;i<num2file.size();i++)
   {
      FileRec & file=*num2file[i];
      bs.writestring(file.name);
      bs.write8(0);
      bs.write8(file.iff_file);
      bs.write32(file.offset);
      bs.write32(file.size);
   }
}
#endif

void
DjVmDir0::decode(ByteStream & bs)
      // WARNING! make sure, that get_size(), encode() and decode() are in sync
{
   name2file.empty();
   num2file.empty();

   for(int i=bs.read16();i>0;i--)
   {
      GUTF8String name;
      char ch;
      while(bs.read(&ch, 1) && ch) name+=ch;
      bool iff_file=bs.read8()?true:false;
      int offset=bs.read32();
      int size=bs.read32();
      add_file(name, iff_file, offset, size);
   };
}

GP<DjVmDir0::FileRec>
DjVmDir0::get_file(const GUTF8String &name)
{
   if (name2file.contains(name))
     return name2file[name];
   return 0;
}

GP<DjVmDir0::FileRec>
DjVmDir0::get_file(int file_num)
{
   if (file_num<num2file.size()) return num2file[file_num];
   return 0;
}

void
DjVmDir0::add_file(
  const GUTF8String &name, bool iff_file, int offset, int size)
{
   DEBUG_MSG("DjVmDir0::add_file(): name='" << name << "', iff=" << iff_file <<
	     ", offset=" << offset << "\n");
   
   if (name.search('/') >= 0)
     G_THROW( ERR_MSG("DjVmDir0.no_slash") );
   
   GP<FileRec> file=new FileRec(name, iff_file, offset, size);
   name2file[name]=file;
   num2file.resize(num2file.size());
   num2file[num2file.size()-1]=file;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
