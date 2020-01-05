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

// From: Leon Bottou, 1/31/2002
// This is purely Lizardtech stuff.

#include "XMLTags.h"
#include "UnicodeByteStream.h"
#include <ctype.h>
#if HAS_WCTYPE
#include <wctype.h>
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

lt_XMLContents::lt_XMLContents(void) {}

lt_XMLContents::lt_XMLContents(GP<lt_XMLTags> t)
{
  tag=t;
}

static GUTF8String
getargn(char const tag[], char const *&t)
{
  char const *s;
  for(s=tag;isspace((unsigned char)(*s));s++);
  for(t=s;(*t)&&((*t)!='/')&&((*t)!='>')&&((*t)!='=')&&!isspace((unsigned char)(*t));++t);
  return GUTF8String(s,t-s);
}

static GUTF8String
getargv(char const tag[], char const *&t)
{
  GUTF8String retval;
  if(tag && tag[0] == '=')
  {
    char const *s=t=tag+1;
    if((*t == '"')||(*t == '\47'))
    {
      char const q=*(t++);
      for(s++;(*t)&&((*t)!=q)&&((*t)!='>');++t);
      retval=GUTF8String(s,t-s);
      if (t[0] == q)
      {
        ++t;
      }
    }else
    {
      for(t=s;(*t)&&((*t)!='/')&&((*t)!='>')&&!isspace((unsigned char)(*t));++t);
      retval=GUTF8String(s,t-s);
    }
  }else
  {
    t=tag;
  }
  return retval;
}

static GUTF8String
tagtoname(char const tag[],char const *&t)
{
  char const *s;
  for(s=tag;isspace((unsigned char)(*s));s++);
  for(t=s;(*t)&&((*t)!='>')&&((*t)!='/')&&!isspace((unsigned char)(*t));++t);
  return GUTF8String(s,t-s);
}

static inline GUTF8String
tagtoname(char const tag[])
{
  char const *t;
  return tagtoname(tag,t);
}

static inline bool
isspaces(const GUTF8String &raw)
{
  return (raw.nextNonSpace() == (int)raw.length());
}

void
lt_XMLTags::ParseValues(char const *t, GMap<GUTF8String,GUTF8String> &args,bool downcase)
{
  GUTF8String argn;
  char const *tt;
  while((argn=getargn(t,tt)).length())
  {
    if(downcase)
      argn=argn.downcase();
    args[argn]=getargv(tt,t).fromEscaped();
  }
}

lt_XMLTags::~lt_XMLTags() {}

lt_XMLTags::lt_XMLTags(void) : startline(0) {}

lt_XMLTags::lt_XMLTags(const char n[]) : startline(0)
{
  char const *t;
  name=tagtoname(n,t);
  ParseValues(t,args);
}

void
lt_XMLTags::init(const GP<ByteStream> &bs)
{
  GP<XMLByteStream> gxmlbs=XMLByteStream::create(bs);
  init(*gxmlbs);
}

void
lt_XMLTags::init(const GURL &url)
{
  const GP<ByteStream> bs=ByteStream::create(url,"rb");
  init(bs);
}

void
lt_XMLTags::init(XMLByteStream &xmlbs)
{
  if(!get_count())
  {
    G_THROW( ERR_MSG("XMLTags.no_GP") );
  }
  GPList<lt_XMLTags> level;
  GUTF8String tag,raw(xmlbs.gets(0,'<',false));
  int linesread=xmlbs.get_lines_read();
  if(!isspaces(raw))
  {
    G_THROW( (ERR_MSG("XMLTags.raw_string") "\t")+raw);
  }
  GUTF8String encoding;
  for(int len;(len=(tag=xmlbs.gets(0,'>',true)).length());)
  {
    if(tag[len-1] != '>')
    {
      G_THROW((ERR_MSG("XMLTags.bad_tag") "\t")+tag);
    }
    switch(tag[1])
    {
      case '?':
      {
        while(len < 4 || tag.substr(len-2,len) != "?>")
        {
          GUTF8String cont(xmlbs.gets(0,'>',true));
          if(!cont.length())
          { 
            G_THROW( (ERR_MSG("XMLTags.bad_PI") "\t")+tag);
          }
          len=((tag+=cont).length());
        }
        char const *n;
        GUTF8String xtag = tag.substr(2,-1);
        GUTF8String xname = tagtoname(xtag,n);
        if(xname.downcase() == "xml")
        {
          ParseValues(n,args);
          for(GPosition pos=args;pos;++pos)
          {
            if(args.key(pos) == "encoding")
            {
              const GUTF8String e=args[pos].upcase();
              if(e != encoding)
              {
                xmlbs.set_encoding((encoding=e));
              }
            }
          }
        }
        break;
      }
      case '!':
      {
        if(tag[2] == '-' && tag[3] == '-')
        {
          while((len < 7) ||
            (tag.substr(len-3,-1) != "-->"))
          {
            GUTF8String cont(xmlbs.gets(0,'>',true));
            if(!cont.length())
            { 
              GUTF8String mesg;
              mesg.format( ERR_MSG("XMLTags.bad_comment") "\t%s",(const char *)tag);
              G_THROW(mesg);
            }
            len=((tag+=cont).length());
          }
        }
        break;
      }
      case '/':
      {
        GUTF8String xname=tagtoname(tag.substr(2,-1));
        GPosition last=level.lastpos();
        if(last)
        {
          if(level[last]->name != xname)
          {
            G_THROW( (ERR_MSG("XMLTags.unmatched_end") "\t")
              +level[last]->name+("\t"+GUTF8String(level[last]->get_Line()))
              +("\t"+xname)+("\t"+GUTF8String(linesread+1)));
          }
          level.del(last);
        }else
        {
          G_THROW( ERR_MSG("XMLTags.bad_form") );
        }
        break;
      }
      default:
      {
        GPosition last=level.lastpos();
        GP<lt_XMLTags> t;
        if(last)
        {
          t=new lt_XMLTags(tag.substr(1,len-1));
          level[last]->addtag(t);
          if(tag[len-2] != '/')
          {
            level.append(t);
          }
        }else if(tag[len-2] != '/')
        {
          char const *n;
          GUTF8String xtag = tag.substr(1,-1); 
          name=tagtoname(xtag, n);
          ParseValues(n,args);
          t=this;
          level.append(t);
        }else
        {
          G_THROW( ERR_MSG("XMLTags.no_body") );
        }
        t->set_Line(linesread+1);
        break;
      }
    }
    if((raw=xmlbs.gets(0,'<',false))[0])
    { 
      linesread=xmlbs.get_lines_read();
      GPosition last=level.lastpos();
      if(last)
      {
        level[last]->addraw(raw);
      }else if(!isspaces(raw))
      {
        G_THROW(( ERR_MSG("XMLTags.raw_string") "\t")+raw);
      }
    }
  }
}

GPList<lt_XMLTags>
lt_XMLTags::get_Tags(char const tagname[]) const
{
  GPosition pos=allTags.contains(tagname);
  GPList<lt_XMLTags> retval;
  return (pos?allTags[pos]:retval);
}

void
lt_XMLTags::get_Maps(char const tagname[],
                     char const argn[],
                     GPList<lt_XMLTags> list,
                     GMap<GUTF8String, GP<lt_XMLTags> > &map)
{
  for(GPosition pos=list;pos;++pos)
  {
    GP<lt_XMLTags> &tag=list[pos];
    if(tag)
    {
      GPosition loc;
      if((loc=tag->contains(tagname)))
      {
        GPList<lt_XMLTags> maps=(GPList<lt_XMLTags> &)((*tag)[loc]);
        for(GPosition mloc=maps;mloc;++mloc)
        {
          GP<lt_XMLTags> gtag=maps[mloc];
          if(gtag)
          {
            GMap<GUTF8String,GUTF8String> &args=gtag->args;
            GPosition gpos;
            if((gpos=args.contains(argn)))
            {
              map[args[gpos]]=gtag;
            }
          }
        }
      }
    }
  }
}

void
lt_XMLTags::write(ByteStream &bs,bool const top) const
{
  if(name.length())
  {
    GUTF8String tag="<"+name;
    for(GPosition pos=args;pos;++pos)
    {
      tag+=GUTF8String(' ')+args.key(pos)+GUTF8String("=\42")+args[pos].toEscaped()+GUTF8String("\42");
    }
    GPosition tags=content;
    if(tags||raw.length()) 
    {
      tag+=">";
      bs.writall((const char *)tag,tag.length());
      tag="</"+name+">";
      if(raw.length())
      {
        bs.writestring(raw);
      }
      for(;tags;++tags)
      {
        content[tags].write(bs);
      }
    }else if(!raw.length())
    {
      tag+="/>";
    }
    bs.writall((const char *)tag,tag.length());
  }
  if(top)
  {
     bs.writall("\n",1);
  }
}

void
lt_XMLContents::write(ByteStream &bs) const
{
  if(tag)
  {
    tag->write(bs,false);
  }
  if(raw.length())
  {
    bs.writestring(raw);
  } 
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
