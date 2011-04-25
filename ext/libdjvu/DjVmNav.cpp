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

#include <ctype.h>

#include "DjVuDocument.h"
#include "DjVmNav.h"
#include "BSByteStream.h"
#include "GURL.h"
#include "debug.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


GP<DjVmNav::DjVuBookMark>
DjVmNav::DjVuBookMark::create(void)
{
  return new DjVuBookMark();
}

GP<DjVmNav::DjVuBookMark>
DjVmNav::DjVuBookMark::create(const unsigned short count,
                              const GUTF8String &displayname, 
                              const GUTF8String &url)
{
  DjVuBookMark *pvm=new DjVuBookMark();
  GP<DjVuBookMark> bookmark=pvm;
  pvm->count=count;
  pvm->displayname=displayname;
  pvm->url=url;
  return bookmark;
}   

DjVmNav::DjVuBookMark::DjVuBookMark(void)
  : count(0), displayname(), url()
{ 
}

GP<DjVmNav>
DjVmNav::create(void)
{
  return new DjVmNav;
}

// Decode the input bytestream and populate this object
void 
DjVmNav::DjVuBookMark::decode(const GP<ByteStream> &gstr)
{
  int textsize=0, readsize=0;
  char *buffer=0;
  ByteStream &bs=*gstr;
  count = bs.read8();
  displayname.empty();
#ifdef DJVMNAV_WITH_256LIMIT
  textsize = bs.read24();
#else
  int counthi = bs.read8();
  count = (counthi<<8)+ count;
  textsize = bs.read16();
#endif
  if (textsize)
    {
      buffer = displayname.getbuf(textsize);
      readsize = bs.read(buffer,textsize);
      buffer[readsize] = 0;
    }
  url.empty();
  textsize = bs.read24();
  if (textsize)
    {
      buffer = url.getbuf(textsize);
      readsize = bs.read(buffer,textsize);
      buffer[readsize] = 0;
    }
}

// Serialize this object to the output bytestream
void  
DjVmNav::DjVuBookMark::encode(const GP<ByteStream> &gstr) 
{
  int textsize=0;
  ByteStream &bs=*gstr;
#ifdef DJVMNAV_WITH_256LIMIT
  if (count>255)
    G_THROW("Excessive number of children in bookmark tree");
  bs.write8(count);
  textsize = displayname.length();
  bs.write24( textsize );
#else
  if (count>65535)
    G_THROW("Excessive number of children in bookmark tree");
  bs.write8( count & 0xff );
  bs.write8( (count>>8) & 0xff );
  textsize = displayname.length();
  bs.write16( textsize );
#endif
  bs.writestring(displayname);
  textsize = url.length();
  bs.write24( textsize );
  bs.writestring(url);
}

// Text dump of this object to the output bytestream
void 
DjVmNav::DjVuBookMark::dump(const GP<ByteStream> &gstr) 
{
  int textsize=0;
  ByteStream &bs=*gstr;
  bs.format("\n  count=%d\n",count);
  textsize = displayname.length();
  bs.format("  (%d) %s\n",textsize, displayname.getbuf());
  textsize = url.length();
  bs.format("  (%d) %s\n",textsize, url.getbuf());
}

// Decode the input bytestream and populate this object
void 
DjVmNav::decode(const GP<ByteStream> &gstr)
{
  //ByteStream &str=*gstr;
  GP<ByteStream> gpBSByteStream = BSByteStream::create(gstr);
  GCriticalSectionLock lock(&class_lock);
  bookmark_list.empty();
  int nbookmarks=gpBSByteStream->read16();
  if (nbookmarks)
    {
      for(int bookmark=0;bookmark<nbookmarks;bookmark++)
        {
          GP<DjVuBookMark> pBookMark=DjVuBookMark::create();
          pBookMark->decode(gpBSByteStream);
          bookmark_list.append(pBookMark);  
        }
    }
}

// Serialize this object to the output stream
void 
DjVmNav::encode(const GP<ByteStream> &gstr)
{
  //ByteStream &str=*gstr;
  GP<ByteStream> gpBSByteStream = BSByteStream::create(gstr, 1024);
  GCriticalSectionLock lock(&class_lock);
  int nbookmarks=bookmark_list.size();
  gpBSByteStream->write16(nbookmarks);
  if (nbookmarks)
    {
      GPosition pos;
      int cnt=0;
      for (pos = bookmark_list; pos; ++pos)
        {
          bookmark_list[pos]->encode(gpBSByteStream);
          cnt++;
        }
      if (nbookmarks != cnt)
        {
          GUTF8String msg;
          msg.format("Corrupt bookmarks found during encode: %d of %d \n",
                     cnt, nbookmarks);
          G_THROW (msg);
        }
    }
}

int 
DjVmNav::getBookMarkCount()
{
  return(bookmark_list.size());
}

void 
DjVmNav::append (const GP<DjVuBookMark> &gpBookMark) 
{
  bookmark_list.append(gpBookMark);
}

bool 
DjVmNav::getBookMark(GP<DjVuBookMark> &gpBookMark, int iPos)
{
  GPosition pos = bookmark_list.nth(iPos);
  if (pos)
    gpBookMark = bookmark_list[pos];
  else
    gpBookMark = 0;
  return (gpBookMark?true:false);
}


// A text dump of this object
void 
DjVmNav::dump(const GP<ByteStream> &gstr)
{
  ByteStream &str=*gstr;
  GCriticalSectionLock lock(&class_lock);
  int nbookmarks=bookmark_list.size();
  str.format("%d bookmarks:\n",nbookmarks);
  if (nbookmarks)
    {
      GPosition pos;
      int cnt=0;
      for (pos = bookmark_list; pos; ++pos)
        {
          bookmark_list[pos]->dump(&str);
          cnt++;
        }
      if (nbookmarks != cnt)
        {
          GUTF8String msg;
          msg.format("Corrupt bookmarks found during encode: %d of %d \n",
                     cnt,nbookmarks);
          G_THROW (msg);
        }
    }
}

bool 
DjVmNav::isValidBookmark()
{
  //test if the bookmark is properly given
  //for example: (4, "A", urla)
  //	         (0, "B", urlb)
  //             (0, "C", urlc)
  //is not a bookmark since A suppose to have 4 decendents, it only get one.
  int bookmark_totalnum=getBookMarkCount();
  GP<DjVuBookMark> gpBookMark;
  int* count_array=(int*)malloc(sizeof(int)*bookmark_totalnum);
  for(int i=0;i<bookmark_totalnum;i++)
    {
      getBookMark(gpBookMark, i);
      count_array[i]=gpBookMark->count;
    }
  int index=0;
  int trees=0;
  int* treeSizes=(int*)malloc(sizeof(int)*bookmark_totalnum);
  while(index<bookmark_totalnum)
    {
      int treeSize=get_tree(index,count_array,bookmark_totalnum);
      if(treeSize>0) //is a tree
        {
          index+=treeSize;
          treeSizes[trees++]=treeSize;
        }
      else //not a tree
        break;
    }
  free(count_array);
  free(treeSizes);
  return true;
}

int 
DjVmNav::get_tree(int index, int* count_array, int count_array_size)
{
  int i=index;
  int accumulate_count=0;
  while(i<count_array_size)
    {
      accumulate_count+=count_array[i];
      if(accumulate_count==0)
        return 1;
      else if(accumulate_count == i-index) //get a tree
        return accumulate_count;
      i++;
    }
  return 0;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
