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

#include "XMLParser.h"
#include "XMLTags.h"
#include "ByteStream.h"
#include "GOS.h"
#include "DjVuDocument.h"
#include "DjVuText.h"
#include "DjVuAnno.h"
#include "DjVuFile.h"
#include "DjVuImage.h"
#include "debug.h"
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

static const char mimetype[]="image/x.djvu";
static const char bodytag[]="BODY";
static const char areatag[]="AREA";
static const char maptag[]="MAP";
static const char objecttag[]="OBJECT";
static const char paramtag[]="PARAM";
static const char charactertag[]="CHARACTER";
static const char wordtag[]="WORD";
static const char linetag[]="LINE";
static const char paragraphtag[]="PARAGRAPH";
static const char regiontag[]="REGION";
static const char pagecolumntag[]="PAGECOLUMN";
static const char hiddentexttag[]="HIDDENTEXT";
static const char metadatatag[]="METADATA";

class lt_XMLParser::Impl : public lt_XMLParser
{
public:
  Impl(void);
  virtual ~Impl();
  /// Parse the specified bytestream.
  virtual void parse(const GP<ByteStream> &bs, GURL *pdjvufile);
  /// Parse the specified tags - this one does all the work
  virtual void parse(const lt_XMLTags &tags, GURL *pdjvufile);
  /// write to disk.
  virtual void save(void);
  /// erase.
  virtual void empty(void);
protected:
  GP<DjVuFile> get_file(const GURL &url,GUTF8String page);

  void parse_anno(const int width, const int height,
    const lt_XMLTags &GObject,
    GMap<GUTF8String,GP<lt_XMLTags> > &Maps, DjVuFile &dfile);

  void parse_text(const int width, const int height,
    const lt_XMLTags &GObject, DjVuFile &dfile);

  void parse_meta(const lt_XMLTags &GObject, DjVuFile &dfile);

  void ChangeAnno( const int width, const int height,
    DjVuFile &dfile, const lt_XMLTags &map);

  void ChangeInfo(DjVuFile &dfile,const int dpi,const double gamma);

  void ChangeText( const int width, const int height,
    DjVuFile &dfile, const lt_XMLTags &map);

  void ChangeMeta( DjVuFile &dfile, const lt_XMLTags &map);

  void ChangeTextOCR( const GUTF8String &value, 
    const int width, const int height,
    const GP<DjVuFile> &dfile);

  // we may want to make these list of modified file static so
  // they only needed to be loaded and saved once.

  GMap<GUTF8String,GP<DjVuFile> > m_files;
  GMap<GUTF8String,GP<DjVuDocument> > m_docs;

  GURL m_codebase; 
  GCriticalSection xmlparser_lock;
};

static GP<ByteStream>
OCRcallback(
  void * const xarg,
  lt_XMLParser::mapOCRcallback * const xcallback,
  const GUTF8String &value=GUTF8String(),
  const GP<DjVuImage> &image=0 );

static inline GP<ByteStream>
OCRcallback(const GUTF8String &value, const GP<DjVuImage> &image)
{
  return OCRcallback(0,0,value,image);
}

lt_XMLParser::lt_XMLParser() {}
lt_XMLParser::~lt_XMLParser() {}
lt_XMLParser::Impl::Impl() {}
lt_XMLParser::Impl::~Impl() {}

GP<lt_XMLParser>
lt_XMLParser::create(void)
{
  return new lt_XMLParser::Impl;
}

// helper function for args
static void 
intList(GUTF8String coords, GList<int> &retval)
{
  int pos=0;
  while(coords.length())
  {
    int epos;
    unsigned long i=coords.toLong(pos,epos,10);
    if(epos>=0)
    {
      retval.append(i);
      const int n=coords.nextNonSpace(epos);
      if(coords[n] != ',')
        break;
      pos=n+1;
    }
  }
}

void 
lt_XMLParser::Impl::empty(void)
{
  GCriticalSectionLock lock(&xmlparser_lock);
  m_files.empty();
  m_docs.empty();
}

void 
lt_XMLParser::Impl::save(void)
{
  GCriticalSectionLock lock(&xmlparser_lock);
  for(GPosition pos=m_docs;pos;++pos)
  {
    const GP<DjVuDocument> doc(m_docs[pos]);
    const GURL url=doc->get_init_url();
    
    DEBUG_MSG("Saving "<<(const char *)url<<" with new text and annotations\n");
    const bool bundle=doc->is_bundled()||(doc->get_doc_type()==DjVuDocument::SINGLE_PAGE);
    doc->save_as(url,bundle);
  }
  empty();
}

void
lt_XMLParser::Impl::parse(const GP<ByteStream> &bs, GURL *pdjvufile)
{
  const GP<lt_XMLTags> tags(lt_XMLTags::create(bs));
  parse(*tags, pdjvufile);
}
  
static const GMap<GUTF8String,GMapArea::BorderType> &
BorderTypeMap(void)
{
  static GMap<GUTF8String,GMapArea::BorderType> typeMap;
  if (! typeMap.size()) 
    {
      typeMap["none"]=GMapArea::NO_BORDER;
      typeMap["xor"]=GMapArea::XOR_BORDER;
      typeMap["solid"]=GMapArea::SOLID_BORDER;
      typeMap["default"]=GMapArea::SOLID_BORDER;
      typeMap["shadowout"]=GMapArea::SHADOW_OUT_BORDER;
      typeMap["shadowin"]=GMapArea::SHADOW_IN_BORDER;
      typeMap["etchedin"]=GMapArea::SHADOW_EIN_BORDER;
      typeMap["etchedout"]=GMapArea::SHADOW_EOUT_BORDER;
    }
  return typeMap;
}

static unsigned long
convertToColor(const GUTF8String &s)
{
  unsigned long retval=0;
  if(s.length())
  {
    int endpos = -1;
    if(s[0] == '#')
    {
      retval=s.substr(1,-1).toULong(0,endpos,16);
    }
    if(endpos < 0)
    {
      G_THROW( (ERR_MSG("XMLAnno.bad_color") "\t")+s );
    }
  }
  return retval;
}

void
lt_XMLParser::Impl::ChangeInfo(DjVuFile &dfile,const int dpi,const double gamma)
{
  GP<DjVuInfo> info;
  if(dpi >= 5 && dpi <= 4800)
  {
    dfile.resume_decode(true);
    if(dfile.info && (dpi != dfile.info->dpi) )
    {
      info=new DjVuInfo(*dfile.info);
      info->dpi=dpi;
    }
  }
  if(gamma >= 0.1 && gamma <= 5.0)
  {
    dfile.resume_decode(true);
    if(dfile.info && (gamma != dfile.info->gamma) )
    {
      if(!info)
        info=new DjVuInfo(*dfile.info);
      info->gamma=gamma;
    }
  }
  if(info)
  {
    dfile.change_info(info);
  }
}

void
lt_XMLParser::Impl::ChangeAnno(
  const int width, const int height,
  DjVuFile &dfile, 
  const lt_XMLTags &map )
{
  dfile.resume_decode(true);
  const GP<DjVuInfo> info(dfile.info);
  const GP<DjVuAnno> ganno(DjVuAnno::create());
  DjVuAnno &anno=*ganno;
  GPosition map_pos;
  map_pos=map.contains(areatag);
  if(dfile.contains_anno())
  {
    GP<ByteStream> annobs=dfile.get_merged_anno();
    if(annobs)
    {
      anno.decode(annobs);
      if(anno.ant && info)
      {
        anno.ant->map_areas.empty();
      }
    }
//    dfile.remove_anno();
  }
  if(info && map_pos)
  {
    const int h=info->height;
    const int w=info->width;
    double ws=1.0;
    double hs=1.0;
    if(width && width != w)
    {
      ws=((double)w)/((double)width); 
    }
    if(height && height != h)
    {
      hs=((double)h)/((double)height); 
    }
    if(!anno.ant)
    {
      anno.ant=DjVuANT::create();
    }
    GPList<GMapArea> &map_areas=anno.ant->map_areas;
    map_areas.empty();
    GPList<lt_XMLTags> gareas=map[map_pos];
    for(GPosition pos=gareas;pos;++pos)
    {
      if(gareas[pos])
      {
        lt_XMLTags &areas=*(gareas[pos]);
        GMap<GUTF8String,GUTF8String> args(areas.get_args());
        GList<int> coords;
        // ******************************************************
        // Parse the coords attribute:  first read the raw data into
        // a list, then scale the x, y data into another list.  For
        // circles, you also get a radius element with (looks like an x
        // with no matching y).
        // ******************************************************
        {
          GPosition coords_pos=args.contains("coords");
          if(coords_pos)
          {
            GList<int> raw_coords;
            intList(args[coords_pos],raw_coords);
            for(GPosition raw_pos=raw_coords;raw_pos;++raw_pos)
            {
              const int r=raw_coords[raw_pos];
              const int x=(int)(ws*(double)r+0.5);
              coords.append(x);
              int y=h-1;
              if(! ++raw_pos)
              {
                y-=(int)(hs*(double)r+0.5);
              }else
              {
                y-=(int)(hs*(double)raw_coords[raw_pos]+0.5);
              }
              coords.append(y);
//            DjVuPrintMessage("Coords (%d,%d)\n",x,y);
            }
          }
        }
        GUTF8String shape;
        {
          GPosition shape_pos=args.contains("shape");
          if(shape_pos)
          {
            shape=args[shape_pos];
          }
        }
        GP<GMapArea> a;
        if(shape == "default")
        {
          GRect rect(0,0,w,h);
          a=GMapRect::create(rect);
        }else if(!shape.length() || shape == "rect")
        {
          int xx[4];
          int i=0;
          for(GPosition rect_pos=coords;(rect_pos)&&(i<4);++rect_pos,++i)
          {
            xx[i]=coords[rect_pos];
          }
          if(i!=4)
          {
            G_THROW( ERR_MSG("XMLAnno.bad_rect") );
          }
          int xmin,xmax; 
          if(xx[0]>xx[2])
          {
            xmax=xx[0];
            xmin=xx[2];
          }else
          {
            xmin=xx[0];
            xmax=xx[2];
          }
          int ymin,ymax; 
          if(xx[1]>xx[3])
          {
            ymax=xx[1];
            ymin=xx[3];
          }else
          {
            ymin=xx[1];
            ymax=xx[3];
          }
          GRect rect(xmin,ymin,xmax-xmin,ymax-ymin);
          a=GMapRect::create(rect);
        }else if(shape == "circle")
        {
          int xx[4];
          int i=0;
          GPosition rect_pos=coords.lastpos();
          if(rect_pos)
          {
            coords.append(coords[rect_pos]);
            for(rect_pos=coords;(rect_pos)&&(i<4);++rect_pos)
            {
              xx[i++]=coords[rect_pos];
            }
          }
          if(i!=4)
          {
            G_THROW( ERR_MSG("XMLAnno.bad_circle") );
          }
          int x=xx[0],y=xx[1],rx=xx[2],ry=(h-xx[3])-1;
          GRect rect(x-rx,y-ry,2*rx,2*ry);
          a=GMapOval::create(rect);
        }else if(shape == "oval")
        {
          int xx[4];
          int i=0;
          for(GPosition rect_pos=coords;(rect_pos)&&(i<4);++rect_pos,++i)
          {
            xx[i]=coords[rect_pos];
          }
          if(i!=4)
          {
            G_THROW( ERR_MSG("XMLAnno.bad_oval") );
          }
          int xmin,xmax; 
          if(xx[0]>xx[2])
          {
            xmax=xx[0];
            xmin=xx[2];
          }else
          {
            xmin=xx[0];
            xmax=xx[2];
          }
          int ymin,ymax; 
          if(xx[1]>xx[3])
          {
            ymax=xx[1];
            ymin=xx[3];
          }else
          {
            ymin=xx[1];
            ymax=xx[3];
          }
          GRect rect(xmin,ymin,xmax-xmin,ymax-ymin);
          a=GMapOval::create(rect);
        }else if(shape == "poly")
        {
          GP<GMapPoly> p=GMapPoly::create();
          for(GPosition poly_pos=coords;poly_pos;++poly_pos)
          {
            int x=coords[poly_pos];
            if(! ++poly_pos)
              break;
            int y=coords[poly_pos];
            p->add_vertex(x,y);
          }
          p->close_poly();
          a=p;
        }else
        {
          G_THROW( ( ERR_MSG("XMLAnno.unknown_shape") "\t")+shape );
        }
        if(a)
        {
          GPosition pos;
          if((pos=args.contains("href")))
          {
            a->url=args[pos];
          }
          if((pos=args.contains("target")))
          {
            a->target=args[pos];
          }
          if((pos=args.contains("alt")))
          {
            a->comment=args[pos];
          }
          if((pos=args.contains("bordertype")))
          {
            GUTF8String b=args[pos];
            static const GMap<GUTF8String,GMapArea::BorderType> typeMap=BorderTypeMap();
            if((pos=typeMap.contains(b)))
            {
              a->border_type=typeMap[pos];
            }else
            {
              G_THROW( (ERR_MSG("XMLAnno.unknown_border") "\t")+b );
            }
          }
          a->border_always_visible=!!args.contains("visible");
          if((pos=args.contains("bordercolor")))
          {
            a->border_color=convertToColor(args[pos]);
          }
          if((pos=args.contains("highlight")))
          {
            a->hilite_color=convertToColor(args[pos]);
          }
          if((pos=args.contains("border")))
          {
             a->border_width=args[pos].toInt(); //atoi(args[pos]);
          }
          map_areas.append(a);
        }
      }
    }
  }
  dfile.set_modified(true);
  dfile.anno=ByteStream::create();
  anno.encode(dfile.anno);
}

GP<DjVuFile>
lt_XMLParser::Impl::get_file(const GURL &url,GUTF8String id)
{
  GP<DjVuFile> dfile;
  GP<DjVuDocument> doc;
  GCriticalSectionLock lock(&xmlparser_lock);
  {
    GPosition pos=m_docs.contains(url.get_string());
    if(pos)
    {
      doc=m_docs[pos];
    }else
    {
      doc=DjVuDocument::create_wait(url);
      if(! doc->wait_for_complete_init())
      {
        G_THROW(( ERR_MSG("XMLAnno.fail_init") "\t")+url.get_string() );
      }
      m_docs[url.get_string()]=doc;
    }
    if(id.is_int())
    {
      const int xpage=id.toInt(); //atoi((char const *)page); 
      if(xpage>0)
        id=doc->page_to_id(xpage-1);
    }else if(!id.length())
    { 
      id=doc->page_to_id(0);
    }
  }
  const GURL fileurl(doc->id_to_url(id));
  GPosition dpos(m_files.contains(fileurl.get_string()));
  if(!dpos)
  {
    if(!doc->get_id_list().contains(id))
    {
      G_THROW( ERR_MSG("XMLAnno.bad_page") );
    }
    dfile=doc->get_djvu_file(id,false);
    if(!dfile)
    {
      G_THROW( ERR_MSG("XMLAnno.bad_page") );
    }
    m_files[fileurl.get_string()]=dfile;
  }else
  {
    dfile=m_files[dpos];
  }
  return dfile;
}
  
void
lt_XMLParser::Impl::parse(const lt_XMLTags &tags, GURL *pdjvufile)
{
  const GPList<lt_XMLTags> Body(tags.get_Tags(bodytag));
  GPosition pos=Body;
 
  if(!pos || (pos != Body.lastpos()))
  {
    G_THROW( ERR_MSG("XMLAnno.extra_body") );
  }
  const GP<lt_XMLTags> GBody(Body[pos]);
  if(!GBody)
  {
    G_THROW( ERR_MSG("XMLAnno.no_body") );
  }

  GMap<GUTF8String,GP<lt_XMLTags> > Maps;
  lt_XMLTags::get_Maps(maptag,"name",Body,Maps);

  const GPList<lt_XMLTags> Objects(GBody->get_Tags(objecttag));
  lt_XMLTags::get_Maps(maptag,"name",Objects,Maps);

  for(GPosition Objpos=Objects;Objpos;++Objpos)
  {
    lt_XMLTags &GObject=*Objects[Objpos];
    // Map of attributes to value (e.g. "width" --> "500")
    const GMap<GUTF8String,GUTF8String> &args=GObject.get_args();
    GURL codebase;
    {
      DEBUG_MSG("Setting up codebase... m_codebase = " << m_codebase << "\n");
      GPosition codebasePos=args.contains("codebase");
      // If user specified a codebase attribute, assume it is correct (absolute URL):
      //  the GURL constructor will throw an exception if it isn't
      if(codebasePos)
      {
        codebase=GURL::UTF8(args[codebasePos]);
      }else if (m_codebase.is_dir())
      {
        codebase=m_codebase;
      }else
      {
        codebase=GURL::Filename::UTF8(GOS::cwd());
      }
      DEBUG_MSG("codebase = " << codebase << "\n");
    }
    // the data attribute specifies the input file.  This can be
    //  either an absolute URL (starts with file:/) or a relative
    //  URL (for now, just a path and file name).  If it's absolute,
    //  our GURL will adequately wrap it.  If it's relative, we need
    //  to use the codebase attribute to form an absolute URL first.
    GPosition datapos=args.contains("data");
    if(datapos)
    {
      GPosition typePos(args.contains("type"));
      if(typePos)
        {
          if(args[typePos] != mimetype)
          continue;
        }
      const GURL url = (pdjvufile) ? *pdjvufile 
        : GURL::UTF8(args[datapos], 
                     (args[datapos][0] == '/') ? codebase.base() : codebase);
      int width;
      {
        GPosition widthPos=args.contains("width");
        width=(widthPos)?args[widthPos].toInt():0;
      }
      int height;
      {
        GPosition heightPos=args.contains("height");
        height=(heightPos)?args[heightPos].toInt():0;
      }
      GUTF8String gamma;
      GUTF8String dpi;
      GUTF8String page;
      GUTF8String do_ocr;
      {
        GPosition paramPos(GObject.contains(paramtag));
        if(paramPos)
        {
          const GPList<lt_XMLTags> Params(GObject[paramPos]);
          for(GPosition loc=Params;loc;++loc)
          {
            const GMap<GUTF8String,GUTF8String> &pargs=Params[loc]->get_args();
            GPosition namepos=pargs.contains("name");
            if(namepos)
            {
              GPosition valuepos=pargs.contains("value");
              if(valuepos)
              {
                const GUTF8String name=pargs[namepos].downcase();
                const GUTF8String &value=pargs[valuepos];
                if(name == "flags")
                {
                  GMap<GUTF8String,GUTF8String> args;
                  lt_XMLTags::ParseValues(value,args,true);
                  if(args.contains("page"))
                  {
                    page=args["page"];
                  }
                  if(args.contains("dpi"))
                  {
                    dpi=args["dpi"];
                  }
                  if(args.contains("gamma"))
                  {
                    gamma=args["gamma"];
                  }
                  if(args.contains("ocr"))
                  {
                    do_ocr=args["ocr"];
                  }
                }else if(name == "page")
                {
                  page=value;
                }else if(name == "dpi")
                {
                  dpi=value;
                }else if(name == "gamma")
                {
                  gamma=value;
                }else if(name == "ocr")
                {
                  do_ocr=value;
                }
              }
            }
          }
        }
      }
      const GP<DjVuFile> dfile(get_file(url,page));
      if(dpi.is_int() || gamma.is_float())
      {
        int pos=0;
        ChangeInfo(*dfile,dpi.toInt(),gamma.toDouble(pos,pos));
      }
      parse_anno(width,height,GObject,Maps,*dfile);
      parse_meta(GObject,*dfile);
      parse_text(width,height,GObject,*dfile);
      ChangeTextOCR(do_ocr,width,height,dfile);
    }
  }
}

void
lt_XMLParser::Impl::parse_anno(
  const int width,
  const int height,
  const lt_XMLTags &GObject,
  GMap<GUTF8String,GP<lt_XMLTags> > &Maps,
  DjVuFile &dfile )
{
  GP<lt_XMLTags> map;
  {
    GPosition usemappos=GObject.get_args().contains("usemap");
    if(usemappos)
    {
      const GUTF8String mapname(GObject.get_args()[usemappos]);
      GPosition mappos=Maps.contains(mapname);
      if(!mappos)
      {
        G_THROW((ERR_MSG("XMLAnno.map_find") "\t")+mapname );
      }else
      {
        map=Maps[mappos];
      }
    }
  }
  if(map)
  {
    ChangeAnno(width,height,dfile,*map);
  }
}

#ifdef max
#undef max
#endif
template<class TYPE>
static inline TYPE max(TYPE a,TYPE b) { return (a>b)?a:b; }
#ifdef min
#undef min
#endif
template<class TYPE>
static inline TYPE min(TYPE a,TYPE b) { return (a<b)?a:b; }

// used to build the zone tree
// true is returned if the GRect is known for this object,
// and false, if the rectangle's size is just the parent size.
static bool
make_child_layer(
  DjVuTXT::Zone &parent,
  const lt_XMLTags &tag, ByteStream &bs,
  const int height, const double ws, const double hs)
{
  bool retval=true;
  // the plugin thinks there are only Pages, Lines and Words
  // so we don't make Paragraphs, Regions and Columns zones
  // if we did the plugin is not able to search the text but 
  // DjVuToText writes out all the text anyway
  DjVuTXT::Zone *self_ptr;
  char sepchar;
  const GUTF8String name(tag.get_name());
  if(name == charactertag)
  {
    self_ptr=parent.append_child();
    self_ptr->ztype = DjVuTXT::CHARACTER;
    sepchar=0;  
  }else if(name == wordtag)
  {
    self_ptr=parent.append_child();
    self_ptr->ztype = DjVuTXT::WORD;
    sepchar=' ';
  }else if(name == linetag)
  {
    self_ptr=parent.append_child();
    self_ptr->ztype = DjVuTXT::LINE;
    sepchar=DjVuTXT::end_of_line;
  }else if(name == paragraphtag)
  {
    self_ptr=parent.append_child();
    self_ptr->ztype = DjVuTXT::PARAGRAPH;
    sepchar=DjVuTXT::end_of_paragraph;
  }else if(name == regiontag)
  {
    self_ptr=parent.append_child();
    self_ptr->ztype = DjVuTXT::REGION;
    sepchar=DjVuTXT::end_of_region;
  }else if(name == pagecolumntag)
  {
    self_ptr=parent.append_child();
    self_ptr->ztype = DjVuTXT::COLUMN;
    sepchar=DjVuTXT::end_of_column;
  }else
  {
    self_ptr = &parent;
    self_ptr->ztype = DjVuTXT::PAGE;
    sepchar=0;
  }
  DjVuTXT::Zone &self = *self_ptr;
  self.text_start = bs.tell();
  int &xmin=self.rect.xmin, &ymin=self.rect.ymin, 
    &xmax=self.rect.xmax, &ymax=self.rect.ymax;
  GRect default_rect;
  default_rect.xmin=max(parent.rect.xmax,parent.rect.xmin);
  default_rect.xmax=min(parent.rect.xmax,parent.rect.xmin);
  default_rect.ymin=max(parent.rect.ymax,parent.rect.ymin);
  default_rect.ymax=min(parent.rect.ymax,parent.rect.ymin);
  // Now if there are coordinates, use those.
  GPosition pos(tag.get_args().contains("coords"));
  if(pos)
  {
    GList<int> rectArgs;
    intList(tag.get_args()[pos], rectArgs);
    if((pos=rectArgs))
    {
      xmin=(int)(ws*(double)rectArgs[pos]);
      if(++pos)
      {
        ymin=(height-1)-(int)(hs*(double)rectArgs[pos]);
        if(++pos)
        {
          xmax=(int)(ws*(double)rectArgs[pos]);
          if(++pos)
          {
            ymax=(height-1)-(int)(hs*(double)rectArgs[pos]);
            if(xmin>xmax) // Make sure xmin is really minimum
            {
              const int t=xmin;
              xmin=xmax;
              xmax=t;
            }
            if(ymin>ymax) // Make sure ymin is really minimum
            {
              const int t=ymin;
              ymin=ymax;
              ymax=t;
            }
          }
        }
      }
    }
  }
  if(self.ztype == DjVuTXT::CHARACTER)
  {
    if(! pos)
    {
      self.rect=default_rect;
      retval=false;
    }
    const GUTF8String raw(tag.get_raw().fromEscaped());
    const int i=raw.nextNonSpace(0);
    bs.writestring(raw.substr(i,raw.firstEndSpace(i)-i));
    if(sepchar)
      bs.write8(sepchar);
    self.text_length = bs.tell() - self.text_start;
  }else if(pos)
  {
    pos=tag.get_content();
    if(pos)
    {
      for(pos=tag.get_content(); pos; ++pos)
      {
        const GP<lt_XMLTags> t(tag.get_content()[pos].tag);
        make_child_layer(self, *t, bs, height,ws,hs);
      }
      if(sepchar)
        bs.write8(sepchar);
      self.text_length = bs.tell() - self.text_start;
    }else
    {
      const GUTF8String raw(tag.get_raw().fromEscaped());
      const int i=raw.nextNonSpace(0);
      bs.writestring(raw.substr(i,raw.firstEndSpace(i)-i));
      if(sepchar)
        bs.write8(sepchar);
      self.text_length = bs.tell() - self.text_start;
    }
  }else
  {
    self.rect=default_rect;
    if((pos=tag.get_content()))
    {
      do
      {
        const GP<lt_XMLTags> t(tag.get_content()[pos].tag);
        const GRect save_rect(self.rect);
        self.rect=default_rect;
	if ((retval = make_child_layer(self, *t, bs, height, ws, hs)))
        {
          xmin=min(save_rect.xmin,xmin);
          xmax=max(save_rect.xmax,xmax);
          ymin=min(save_rect.ymin,ymin);
          ymax=max(save_rect.ymax,ymax);
        }else
        {
          // If the child doesn't have coordinates, we need to use a box
          // at least as big as the parent's coordinates.
          xmin=min(save_rect.xmin,default_rect.xmax);
          xmax=max(save_rect.xmax,default_rect.xmin);
          ymin=min(save_rect.ymin,default_rect.ymax);
          ymax=max(save_rect.ymax,default_rect.ymin);
          for(; pos; ++pos)
          {
            const GP<lt_XMLTags> t(tag.get_content()[pos].tag);
            make_child_layer(self, *t, bs, height,ws,hs);
          }
          break;
        }
      } while(++pos);
      if(sepchar)
        bs.write8(sepchar);
      self.text_length = bs.tell() - self.text_start;
    }else
    {
      const GUTF8String raw(tag.get_raw().fromEscaped());
      const int i=raw.nextNonSpace(0);
      bs.writestring(raw.substr(i,raw.firstEndSpace(i)-i));
      if(sepchar)
        bs.write8(sepchar);
      self.text_length = bs.tell() - self.text_start;
    }
  }
  parent.rect.xmin=min(xmin,parent.rect.xmin);
  parent.rect.ymin=min(ymin,parent.rect.ymin);
  parent.rect.xmax=max(xmax,parent.rect.xmax);
  parent.rect.ymax=max(ymax,parent.rect.ymax);
  if(xmin>xmax)
  {
    const int t=xmin;
    xmin=xmax;
    xmax=t;
  }
  if(ymin>ymax)
  {
    const int t=ymin;
    ymin=ymax;
    ymax=t;
  }
//  DjVuPrintMessage("(%d,%d)(%d,%d)<<<\\%o>>>\n",
//    xmin,ymin,xmax,ymax, sepchar);
  return retval;
}

void 
lt_XMLParser::Impl::ChangeTextOCR(
  const GUTF8String &value,
  const int width,
  const int height,
  const GP<DjVuFile> &dfile)
{
  if(value.length() && value.downcase() != "false")
  {
    const GP<ByteStream> bs=OCRcallback(value,DjVuImage::create(dfile));
    if( bs && bs->size() )
    {
      const GP<lt_XMLTags> tags(lt_XMLTags::create(bs));
      ChangeText(width,height,*dfile,*tags);
    }
  }
}

void 
lt_XMLParser::Impl::ChangeMeta(
  DjVuFile &dfile, const lt_XMLTags &tags )
{
  dfile.resume_decode(true);
  GP<ByteStream> gbs(ByteStream::create());
  tags.write(*gbs,false);
  gbs->seek(0L);
  GUTF8String raw(gbs->getAsUTF8());
  if(raw.length())
  {
     //GUTF8String gs="<"+(metadatatag+(">"+raw))+"</"+metadatatag+">\n");
    dfile.change_meta(raw+"\n");
  }else
  {
    dfile.change_meta(GUTF8String());
  }
}

void 
lt_XMLParser::Impl::ChangeText(
  const int width, const int height,
  DjVuFile &dfile, const lt_XMLTags &tags )
{
  dfile.resume_decode(true);
  
  GP<DjVuText> text = DjVuText::create();
  GP<DjVuTXT> txt = text->txt = DjVuTXT::create();
  
  // to store the new text
  GP<ByteStream> textbs = ByteStream::create(); 
  
  GP<DjVuInfo> info=(dfile.info);
  if(info)
  {
    const int h=info->height;
    const int w=info->width;
    txt->page_zone.text_start = 0;
    DjVuTXT::Zone &parent=txt->page_zone;
    parent.rect.xmin=0;
    parent.rect.ymin=0;
    parent.rect.ymax=h;
    parent.rect.xmax=w;
    double ws=1.0;
    if(width && width != w)
    {
      ws=((double)w)/((double)width);
    }
    double hs=1.0;
    if(height && height != h)
    {
      hs=((double)h)/((double)height);
    }
    make_child_layer(parent, tags, *textbs, h, ws,hs);
    textbs->write8(0);
    long len = textbs->tell();
    txt->page_zone.text_length = len;
    textbs->seek(0,SEEK_SET);
    textbs->read(txt->textUTF8.getbuf(len), len);
  
    dfile.change_text(txt,false);
  }
}

void
lt_XMLParser::Impl::parse_text(
  const int width,
  const int height,
  const lt_XMLTags &GObject,
  DjVuFile &dfile )
{
  GPosition textPos = GObject.contains(hiddentexttag);
  if(textPos)
  {
    // loop through the hidden text - there should only be one 
    // if there are more ??only the last one will be saved??
    GPList<lt_XMLTags> textTags = GObject[textPos];
    GPosition pos = textTags;
    ChangeText(width,height,dfile,*textTags[pos]);
  }
}

void
lt_XMLParser::Impl::parse_meta(
  const lt_XMLTags &GObject,
  DjVuFile &dfile )
{
  GPosition metaPos = GObject.contains(metadatatag);
  if(metaPos)
  {
    // loop through the hidden text - there should only be one 
    // if there are more ??only the last one will be saved??
    GPList<lt_XMLTags> metaTags = GObject[metaPos];
    GPosition pos = metaTags;
    ChangeMeta(dfile,*metaTags[pos]);
  }
}

static GP<ByteStream>
OCRcallback(
  void * const xarg,
  lt_XMLParser::mapOCRcallback * const xcallback,
  const GUTF8String &value,
  const GP<DjVuImage> &image )
{
  GP<ByteStream> retval;
  static void *arg=0;
  static lt_XMLParser::mapOCRcallback *callback=0;
  if(image)
  {
    if(callback)
      retval=callback(arg,value,image);
  }else
  {
    arg=xarg;
    callback=xcallback;
  }
  return retval;
}

void
lt_XMLParser::setOCRcallback(
  void * const arg,
  mapOCRcallback * const callback)
{
  ::OCRcallback(arg,callback);
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
