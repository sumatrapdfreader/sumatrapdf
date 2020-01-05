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

#include "DjVuAnno.h"
#include "GContainer.h"
#include "GException.h"
#include "IFFByteStream.h"
#include "BSByteStream.h"
#include "GMapAreas.h"

#include "debug.h"

#include <ctype.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


// GLParser.h and GLParser.cpp used to be separate files capable to decode
// that weird ANTa chunk format into C++ structures and lists. But since
// its implementation is temporary and is used only in this file (DjVuAnno.cpp)
// it appears reasonable to build it in here.

//***************************************************************************
//****************************** GLParser.h *********************************
//***************************************************************************


class GLObject : public GPEnabled
{
public:
   enum GLObjectType { INVALID=0, NUMBER=1, STRING=2, SYMBOL=3, LIST=4 };
   static const char * const GLObjectString[LIST+1];

   GLObject(int _number=0);
   GLObject(GLObjectType type, const char * str);
   GLObject(const char * name, const GPList<GLObject> & list);
   virtual ~GLObject(void);
   
   int		get_number(void) const;
   GUTF8String	get_string(void) const;
   GUTF8String	get_symbol(void) const;
   GPList<GLObject>	& get_list(void);
   GP<GLObject>	operator[](int n) const;
   
   GLObjectType	get_type(void) const;
   GUTF8String	get_name(void) const;
   void		print(ByteStream & str, int compact=1, int indent=0, int * cur_pos=0) const;
private:
   GLObjectType	type;
   GUTF8String	name;
   
   int		number;
   GUTF8String	string;
   GUTF8String	symbol;
   GPList<GLObject>	list;
   void throw_can_not_convert_to(const GLObjectType to) const;
};

const char * const GLObject::GLObjectString[]=
  {"invalid", "number", "string", "symbol", "list"};

inline GLObject::GLObjectType
GLObject::get_type(void) const { return type; }

inline
GLObject::~GLObject(void) {}

class GLToken
{
public:
   enum GLTokenType { OPEN_PAR, CLOSE_PAR, OBJECT };
   GLTokenType	type;
   GP<GLObject>	object;
   
   GLToken(GLTokenType type, const GP<GLObject> & object);
};

inline
GLToken::GLToken(GLTokenType xtype, const GP<GLObject> & xobject) :
      type(xtype), object(xobject) {}

class GLParser
{
public:
   void		parse(const char * str);
   GPList<GLObject>	& get_list(void);
   GP<GLObject>		get_object(const char * name, bool last=true);
   void		print(ByteStream & str, int compact=1);

   GLParser(void);
   GLParser(const char * str);
   ~GLParser(void);
private:
   GPList<GLObject>	list;

   bool   	compat;
   void		skip_white_space(const char * & start);
   void 	check_compat(const char *str);
   GLToken	get_token(const char * & start);
   void		parse(const char * cur_name, GPList<GLObject> & list,
		      const char * & start);
};

GLParser::GLParser(void) 
  : compat(false)
{
}

GLParser::~GLParser(void) 
{
}

GPList<GLObject> &
GLParser::get_list(void) 
{ 
  return list; 
}

GLParser::GLParser(const char * str) 
  : compat(false)
{
  parse(str); 
}


//***************************************************************************
//***************************** GLParser.cpp ********************************
//***************************************************************************


GLObject::GLObject(int xnumber) : type(NUMBER), number(xnumber) {}

GLObject::GLObject(GLObjectType xtype, const char * str) : type(xtype)
{
   if (type!=STRING && type!=SYMBOL)
      G_THROW( ERR_MSG("DjVuAnno.bad_type") );
   if (type==STRING) 
      string=str;
   else symbol=str;
}

GLObject::GLObject(const char * xname, const GPList<GLObject> & xlist) :
      type(LIST), name(xname), list(xlist) {}


static GUTF8String make_c_string(GUTF8String string)
{
  GUTF8String buffer;
  const char *data = (const char*)string;
  int length = string.length();
  buffer = GUTF8String("\"");
  while (*data && length>0) 
    {
      int span = 0;
      while (span<length && (unsigned char)(data[span])>=0x20 && 
             data[span]!=0x7f && data[span]!='"' && data[span]!='\\' )
        span++;
      if (span > 0) 
        {  
          buffer = buffer + GUTF8String(data, span);
          data += span;
          length -= span;
        }  
      else 
        {
          char buf[8];
          static const char *tr1 = "\"\\tnrbf";
          static const char *tr2 = "\"\\\t\n\r\b\f";
          sprintf(buf,"\\%03o", (int)(((unsigned char*)data)[span]));
          for (int i=0; tr2[i]; i++)
            if (data[span] == tr2[i])
              buf[1] = tr1[i];
          if (buf[1]<'0' || buf[1]>'3')
            buf[2] = 0;
          buffer = buffer + GUTF8String(buf);
          data += 1;
          length -= 1;
        }
    }
  buffer = buffer + GUTF8String("\"");
  return buffer;
}


void
GLObject::print(ByteStream & str, int compact, int indent, int * cur_pos) const
{
  int local_cur_pos = 0;
  if (!cur_pos) { cur_pos = &local_cur_pos; }
  
  GUTF8String buffer;
  switch(type)
  {
  case NUMBER:
    buffer.format("%d",number);
    break;
  case STRING:
    buffer = make_c_string(string);
    break;
  case SYMBOL:
    buffer.format("%s",(const char *)symbol);
    break;
  case LIST:
    buffer.format("(%s",(const char *)name);
    break;
  case INVALID:
    break;
  }
  const char * to_print = (const char*)buffer;
  if (!compact && *cur_pos+strlen(to_print)>70)
  {
    char ch='\n';
    str.write(&ch, 1);
    ch=' ';
    for(int i=0;i<indent;i++) str.write(&ch, 1);
    *cur_pos=indent;
  }
  str.write(to_print, strlen(to_print));
  char ch=' ';
  str.write(&ch, 1);
  *cur_pos+=strlen(to_print)+1;
  if (type==LIST)
  {
    int indent=*cur_pos-strlen(to_print);
    for(GPosition pos=list;pos;++pos)
      list[pos]->print(str, compact, indent, cur_pos);
    str.write(") ", 2);
    *cur_pos+=2;
  }
}

//  This function constructs message names for external lookup.
//  The message names are constructed to avoid the problems of concatenating
//  phrases (which does not translate well into other languages). The
//  message names that can be generated are (listed here to appease the
//  auditing program which reads comments):
//    ERR_MSG("DjVuAnno.invalid2number"), ERR_MSG("DjVuAnno.string2number"),
//    ERR_MSG("DjVuAnno.symbol2number"), ERR_MSG("DjVuAnno.list2number")
//    ERR_MSG("DjVuAnno.invalid2string"), ERR_MSG("DjVuAnno.number2string"),
//    ERR_MSG("DjVuAnno.symbol2string"), ERR_MSG("DjVuAnno.list2string")
//    ERR_MSG("DjVuAnno.invalid2symbol"), ERR_MSG("DjVuAnno.number2symbol"),
//    ERR_MSG("DjVuAnno.string2symbol"), ERR_MSG("DjVuAnno.list2symbol")
//    ERR_MSG("DjVuAnno.invalid2list"), ERR_MSG("DjVuAnno.number2list"),
//    ERR_MSG("DjVuAnno.string2list"), ERR_MSG("DjVuAnno.symbol2list")
void
GLObject::throw_can_not_convert_to(const GLObjectType to) const
{
  static const GUTF8String two('2');
  static const GUTF8String tab('\t');
  GUTF8String mesg("DjVuAnno.");
  switch(type)
  {
    case NUMBER:
      mesg+=GLObjectString[NUMBER]+two+GLObjectString[to]+tab+GUTF8String(number);
      break;
    case STRING:
      mesg+=GLObjectString[STRING]+two+GLObjectString[to]+tab+string;
      break;
    case SYMBOL:
      mesg+=GLObjectString[SYMBOL]+two+GLObjectString[to]+tab+symbol;
      break;
    case LIST:
      mesg+=GLObjectString[LIST]+two+GLObjectString[to]+tab+name;
      break;
    default:
      mesg+=GLObjectString[INVALID]+two+GLObjectString[to];
      break;
  }
  G_THROW(mesg);
}

GUTF8String
GLObject::get_string(void) const
{
   if (type!=STRING)
   {
      throw_can_not_convert_to(STRING);
   }
   return string;
}

GUTF8String
GLObject::get_symbol(void) const
{
   if (type!=SYMBOL)
   {
      throw_can_not_convert_to(SYMBOL);
   }
   return symbol;
}

int
GLObject::get_number(void) const
{
   if (type!=NUMBER)
   {
      throw_can_not_convert_to(NUMBER);
   }
   return number;
}

GUTF8String
GLObject::get_name(void) const
{
   if (type!=LIST)
   {
      throw_can_not_convert_to(LIST);
   }
   return name;
}

GP<GLObject>
GLObject::operator[](int n) const
{
   if (type!=LIST)
   {
      throw_can_not_convert_to(LIST);
   }
   if (n>=list.size()) G_THROW( ERR_MSG("DjVuAnno.too_few") "\t"+name);
   int i;
   GPosition pos;
   for(i=0, pos=list;i<n && pos;i++, ++pos)
   		continue;
   return list[pos];
}

GPList<GLObject> &
GLObject::get_list(void)
{
   if (type!=LIST)
   {
      throw_can_not_convert_to(LIST);
   }
   return list;
}

//********************************** GLParser *********************************

void
GLParser::skip_white_space(const char * & start)
{
  while(*start && isspace((unsigned char)(*start))) start++;
   if (!*start) 
       G_THROW( ByteStream::EndOfFile );
}

GLToken
GLParser::get_token(const char * & start)
{
   skip_white_space(start);
   char c = *start;
   if (c == '(')
     {
       start++;
       return GLToken(GLToken::OPEN_PAR, 0);
     }
   else if (c == ')')
     {
       start++;
       return GLToken(GLToken::CLOSE_PAR, 0);
     }
   else if (c == '"')
     {
       GUTF8String str;
       start++;
       while(1)
	 {
           int span = 0;
           while (start[span] && start[span]!='\\' && start[span]!='\"')
             span++;
           if (span > 0)
             {
               str = str + GUTF8String(start,span);
               start += span;
             }
           else if (start[0]=='\"')
             {
               start += 1;
               break;
             }
           else if (start[0]=='\\' && compat)
             {
               char c = start[1];
               if (c == '\"')
                 {
                   start += 2;
                   str += '\"';
                 }
               else
                 {
                   start += 1;
                   str += '\\';
                 }
             }
           else if (start[0]=='\\' && start[1])
             {
               char c = *++start;
               if (c>='0' && c<='7')
                 {
                   int x = 0;
                   for (int i=0; i<3 && c>='0' && c<='7'; i++) 
                     {
                       x = x * 8 + c - '0';
                       c = *++start;
                     }
                   str += (char)(x & 0xff);
                 }
               else
                 {
                   static const char *tr1 = "tnrbfva";
                   static const char *tr2 = "\t\n\r\b\f\013\007";
                   for (int i=0; tr1[i]; i++)
                     if (c == tr1[i])
                       c = tr2[i];
                   start += 1;
                   str += c;
                 }
             }
           else 
             {
               G_THROW( ByteStream::EndOfFile );
             }
         }
       return GLToken(GLToken::OBJECT, new GLObject(GLObject::STRING, str));
     }
   else if (c=='-' || (c>='0' && c<='9'))
     {
       const char *here = start;
       long val = strtol(start, (char**) &start, 10);
       if (start > here)
         return GLToken(GLToken::OBJECT, new GLObject(val));
     }
   
       GUTF8String str;
   while(c != 0 && c != ')' && c != '(' && c != '"' && !isspace((unsigned char)c))
	 {
       str += c;
       c = *++start;
	 }
   if (c == 0)
     G_THROW(ByteStream::EndOfFile);
   else
       return GLToken(GLToken::OBJECT, new GLObject(GLObject::SYMBOL, str));
} 

void
GLParser::parse(const char * cur_name, GPList<GLObject> & list,
		const char * & start)
{
  DEBUG_MSG("GLParse::parse(): Parsing contents of object '" << cur_name << "'\n");
  DEBUG_MAKE_INDENT(3);
  
  while(1)
  {
    GLToken token=get_token(start);
    if (token.type==GLToken::OPEN_PAR)
    {
      if (isspace((unsigned char)(*start)))
      {
        GUTF8String mesg=GUTF8String( ERR_MSG("DjVuAnno.paren") "\t")+cur_name;
        G_THROW(mesg);
      }
      
      GLToken tok=get_token(start);
      GP<GLObject> object=tok.object;	// This object should be SYMBOL
      // We will convert it to LIST later
      if (tok.type!=GLToken::OBJECT || object->get_type()!=GLObject::SYMBOL)
      {
        if (tok.type==GLToken::OPEN_PAR ||
          tok.type==GLToken::CLOSE_PAR)
        {
          GUTF8String mesg=GUTF8String( ERR_MSG("DjVuAnno.no_paren") "\t")+cur_name;
          G_THROW(mesg);
        }
        if (tok.type==GLToken::OBJECT)
        {
          GLObject::GLObjectType type=object->get_type();
          if (type==GLObject::NUMBER)
          {
            GUTF8String mesg( ERR_MSG("DjVuAnno.no_number") "\t");
            mesg += cur_name;
            G_THROW(mesg);
          }
          else if (type==GLObject::STRING)
          {
            GUTF8String mesg( ERR_MSG("DjVuAnno.no_string") "\t");
            mesg += cur_name;
            G_THROW(mesg);
          }
        }
      }
      
      // OK. Get the object contents
      GPList<GLObject> new_list;
      G_TRY
      {
        parse(object->get_symbol(), new_list, start);
      } 
      G_CATCH(exc)
      {
        if (exc.cmp_cause(ByteStream::EndOfFile))
          G_RETHROW;
      } 
      G_ENDCATCH;
      list.append(new GLObject(object->get_symbol(), new_list));
      continue;
    }
    if (token.type==GLToken::CLOSE_PAR) 
      return;
    list.append(token.object);
  }
}

void 
GLParser::check_compat(const char *s)
{
  int state = 0;
  while (s && *s && !compat)
    {
      switch(state)
        {
        case 0:
          if (*s == '\"')
            state = '\"';
          break;
        case '\"':
          if (*s == '\"')
            state = 0;
          else if (*s == '\\')
            state = '\\';
          else if ((unsigned char)(*s)<0x20 || *s==0x7f)
            compat = true;
          break;
        case '\\':
          if (!strchr("01234567tnrbfva\"\\",*s))
            compat = true;
          state = '\"';
          break;
        }
      s += 1;
    }
}

void
GLParser::parse(const char * str)
{
   DEBUG_MSG("GLParser::parse(): parsing string contents\n");
   DEBUG_MAKE_INDENT(3);
   
   G_TRY
   {
      check_compat(str);
      parse("toplevel", list, str);
   } G_CATCH(exc)
   {
      if (exc.cmp_cause(ByteStream::EndOfFile))
        G_RETHROW;
   } G_ENDCATCH;
}

void
GLParser::print(ByteStream & str, int compact)
{
   for(GPosition pos=list;pos;++pos)
      list[pos]->print(str, compact);
}

GP<GLObject>
GLParser::get_object(const char * name, bool last)
{
   GP<GLObject> object;
   for(GPosition pos=list;pos;++pos)
   {
      GP<GLObject> obj=list[pos];
      if (obj->get_type()==GLObject::LIST &&
	  obj->get_name()==name)
      {
	 object=obj;
	 if (!last) break;
      }
   }
   return object;
}

//***************************************************************************
//********************************** ANT ************************************
//***************************************************************************

static const char *zoom_strings[]={
  "default","page","width","one2one","stretch"};
static const int zoom_strings_size=sizeof(zoom_strings)/sizeof(const char *);

static const char *mode_strings[]={
  "default","color","fore","back","bw"};
static const int mode_strings_size=sizeof(mode_strings)/sizeof(const char *);

static const char *align_strings[]={
  "default","left","center","right","top","bottom"};
static const int align_strings_size=sizeof(align_strings)/sizeof(const char *);

#define PNOTE_TAG	"pnote"
#define BACKGROUND_TAG	"background"
#define ZOOM_TAG	"zoom"
#define MODE_TAG	"mode"
#define ALIGN_TAG	"align"
#define HALIGN_TAG	"halign"
#define VALIGN_TAG	"valign"
#define METADATA_TAG    "metadata"
#define XMP_TAG         "xmp"

static const unsigned long default_bg_color=0xffffffff;

DjVuANT::DjVuANT(void)
{
   bg_color=default_bg_color;
   zoom=0;
   mode=MODE_UNSPEC;
   hor_align=ver_align=ALIGN_UNSPEC;
}

DjVuANT::~DjVuANT()
{
}

GUTF8String
DjVuANT::get_paramtags(void) const
{
  GUTF8String retval;
  if(zoom > 0)
  {
    retval+="<PARAM name=\"" ZOOM_TAG "\" value=\""+GUTF8String(zoom)+="\" />\n";
  }else if(zoom && ((-zoom)<zoom_strings_size))
  {
    retval+="<PARAM name=\"" ZOOM_TAG "\" value=\""+GUTF8String(zoom_strings[-zoom])+"\" />\n";
  }
  if((mode>0)&&(mode<mode_strings_size))
  {
    retval+="<PARAM name=\"" MODE_TAG "\" value=\""+GUTF8String(mode_strings[mode])+"\" />\n";
  }
  if((hor_align>ALIGN_UNSPEC)&&(hor_align<align_strings_size))
  {
    retval+="<PARAM name=\"" HALIGN_TAG "\" value=\""+GUTF8String(align_strings[hor_align])+"\" />\n";
  }
  if((ver_align>ALIGN_UNSPEC)&&(ver_align<align_strings_size))
  {
    retval+="<PARAM name=\"" VALIGN_TAG "\" value=\""+GUTF8String(align_strings[ver_align])+"\" />\n";
  }
  if((bg_color&0xffffff) == bg_color)
  {
    retval+="<PARAM name=\"" BACKGROUND_TAG "\" value=\""+GUTF8String().format("#%06lX",bg_color)+"\" />\n";
  }
  return retval;
}

void
DjVuANT::writeParam(ByteStream &str_out) const
{
  str_out.writestring(get_paramtags());
}

GUTF8String
DjVuANT::get_xmlmap(const GUTF8String &name,const int height) const
{
  GUTF8String retval("<MAP name=\""+name.toEscaped()+"\" >\n");
  for(GPosition pos(map_areas);pos;++pos)
  {
    retval+=map_areas[pos]->get_xmltag(height);
  }
  return retval+"</MAP>\n";
}

void
DjVuANT::writeMap(
  ByteStream &str_out,const GUTF8String &name,const int height) const
{
  str_out.writestring("<MAP name=\""+name.toEscaped()+"\" >\n");
  for(GPosition pos(map_areas);pos;++pos)
  {
    str_out.writestring(GUTF8String(map_areas[pos]->get_xmltag(height)));
  }
  str_out.writestring(GUTF8String("</MAP>\n"));
}

GUTF8String
DjVuANT::read_raw(ByteStream & str)
{
   GUTF8String raw;
   char buffer[1024];
   int length;
   while((length=str.read(buffer, 1024)))
      raw+=GUTF8String(buffer, length);
   return raw;
}

void
DjVuANT::decode(class GLParser & parser)
{
   bg_color=get_bg_color(parser);
   zoom=get_zoom(parser);
   mode=get_mode(parser);
   hor_align=get_hor_align(parser);
   ver_align=get_ver_align(parser);
   map_areas=get_map_areas(parser);
   metadata=get_metadata(parser); 
   xmpmetadata=get_xmpmetadata(parser);
}


void 
DjVuANT::decode(ByteStream & str)
{
   GLParser parser(read_raw(str));
   decode(parser);
}

void
DjVuANT::merge(ByteStream & str)
{
   GLParser parser(encode_raw());
   GUTF8String add_raw=read_raw(str);
   parser.parse(add_raw);
   decode(parser);
}

void
DjVuANT::encode(ByteStream &bs)
{
  GUTF8String raw=encode_raw();
  bs.writall((const char*) raw, raw.length());
}

unsigned int 
DjVuANT::get_memory_usage() const
{
  return sizeof(DjVuANT);
}

unsigned char
DjVuANT::decode_comp(char ch1, char ch2)
{
   unsigned char dig1=0;
   if (ch1)
   {
      ch1=toupper(ch1);
      if (ch1>='0' && ch1<='9') dig1=ch1-'0';
      if (ch1>='A' && ch1<='F') dig1=10+ch1-'A';
      
      unsigned char dig2=0;
      if (ch2)
      {
	 ch2=toupper(ch2);
	 if (ch2>='0' && ch2<='9') dig2=ch2-'0';
	 if (ch2>='A' && ch2<='F') dig2=10+ch2-'A';
	 return (dig1 << 4) | dig2;
      }
      return dig1;
   }
   return 0;
}

unsigned long int
DjVuANT::cvt_color(const char * color, unsigned long int def)
{
   if (color[0]!='#') return def;

   unsigned long int color_rgb=0;
   color++;
   const char * start, * end;
   
      // Do blue
   end=color+strlen(color); start=end-2;
   if (start<color) start=color;
   if (end>start)
      color_rgb|=decode_comp(start[0], start+1<end ? start[1] : 0);
   
      // Do green
   end=color+strlen(color)-2; start=end-2;
   if (start<color) start=color;
   if (end>start)
      color_rgb|=decode_comp(start[0], start+1<end ? start[1] : 0) << 8;
   
      // Do red
   end=color+strlen(color)-4; start=end-2;
   if (start<color) start=color;
   if (end>start)
      color_rgb|=decode_comp(start[0], start+1<end ? start[1] : 0) << 16;

      // Do the fourth byte
   end=color+strlen(color)-6; start=end-2;
   if (start<color) start=color;
   if (end>start)
      color_rgb|=decode_comp(start[0], start+1<end ? start[1] : 0) << 24;
   
   return color_rgb;
}

unsigned long int
DjVuANT::get_bg_color(GLParser & parser)
{
  unsigned long retval=default_bg_color;
  DEBUG_MSG("DjVuANT::get_bg_color(): getting background color ...\n");
  DEBUG_MAKE_INDENT(3);
  G_TRY
  {
    GP<GLObject> obj=parser.get_object(BACKGROUND_TAG);
    if (obj && obj->get_list().size()==1)
    {
      GUTF8String color=(*obj)[0]->get_symbol();
      DEBUG_MSG("color='" << color << "'\n");
      retval=cvt_color(color, 0xffffff);
    }
#ifndef NDEBUG
    if(retval == default_bg_color)
    {
      DEBUG_MSG("can't find any.\n");
    }
#endif // NDEBUG
  } G_CATCH_ALL {} G_ENDCATCH;
#ifndef NDEBUG
  if(retval == default_bg_color)
  {
    DEBUG_MSG("resetting color to 0xffffffff (UNSPEC)\n");
  }
#endif // NDEBUG
  return retval;
}

int
DjVuANT::get_zoom(GLParser & parser)
      // Returns:
      //   <0 - special zoom (like ZOOM_STRETCH)
      //   =0 - not set
      //   >0 - numeric zoom (%%)
{
  int retval=ZOOM_UNSPEC;
  DEBUG_MSG("DjVuANT::get_zoom(): getting zoom factor ...\n");
  DEBUG_MAKE_INDENT(3);
  G_TRY
  {
    GP<GLObject> obj=parser.get_object(ZOOM_TAG);
    if (obj && obj->get_list().size()==1)
    {
      const GUTF8String zoom((*obj)[0]->get_symbol());
      DEBUG_MSG("zoom='" << zoom << "'\n");
     
      for(int i=0;(i<zoom_strings_size);++i)
      {
        if(zoom == zoom_strings[i])
        {
          retval=(-i);
          break;
        }
      }
      if(retval == ZOOM_UNSPEC)
      {
        if (zoom[0]!='d')
        {
          G_THROW( ERR_MSG("DjVuAnno.bad_zoom") );
        }else
        {
          retval=zoom.substr(1, zoom.length()).toInt(); //atoi((const char *) zoom+1);
        }
      }
    }
#ifndef NDEBUG
    if(retval == ZOOM_UNSPEC)
    {
      DEBUG_MSG("can't find any.\n");
    }
#endif // NDEBUG
  } G_CATCH_ALL {} G_ENDCATCH;
#ifndef NDEBUG
  if(retval == ZOOM_UNSPEC)
  {
    DEBUG_MSG("resetting zoom to 0 (UNSPEC)\n");
  }
#endif // NDEBUG
  return retval;
}

int
DjVuANT::get_mode(GLParser & parser)
{
  int retval=MODE_UNSPEC;
  DEBUG_MSG("DjVuAnt::get_mode(): getting default mode ...\n");
  DEBUG_MAKE_INDENT(3);
  G_TRY
  {
    GP<GLObject> obj=parser.get_object(MODE_TAG);
    if (obj && obj->get_list().size()==1)
    {
      const GUTF8String mode((*obj)[0]->get_symbol());
      DEBUG_MSG("mode='" << mode << "'\n");
      for(int i=0;(i<mode_strings_size);++i)
      {
        if(mode == mode_strings[i])
        {
          retval=i;
          break;
        }
      }
    }
#ifndef NDEBUG
    if(retval == MODE_UNSPEC)
    {
      DEBUG_MSG("can't find any.\n");
    }
#endif // NDEBUG
  } G_CATCH_ALL {} G_ENDCATCH;
#ifndef NDEBUG
  if(retval == MODE_UNSPEC)
  {
    DEBUG_MSG("resetting mode to MODE_UNSPEC\n");
  }
#endif // NDEBUG
  return retval;
}

static inline DjVuANT::alignment
legal_halign(const int i)
{
  DjVuANT::alignment retval;
  switch((DjVuANT::alignment)i)
  {
  case DjVuANT::ALIGN_LEFT:
  case DjVuANT::ALIGN_CENTER:
  case DjVuANT::ALIGN_RIGHT:
    retval=(DjVuANT::alignment)i;
    break;
  default:
    retval=DjVuANT::ALIGN_UNSPEC;
    break;
  }
  return retval;
}

static inline DjVuANT::alignment
legal_valign(const int i)
{
  DjVuANT::alignment retval;
  switch((DjVuANT::alignment)i)
  {
  case DjVuANT::ALIGN_CENTER:
  case DjVuANT::ALIGN_TOP:
  case DjVuANT::ALIGN_BOTTOM:
    retval=(DjVuANT::alignment)i;
    break;
  default:
    retval=DjVuANT::ALIGN_UNSPEC;
    break;
  }
  return retval;
}

DjVuANT::alignment
DjVuANT::get_hor_align(GLParser & parser)
{
  DEBUG_MSG("DjVuAnt::get_hor_align(): getting hor page alignemnt ...\n");
  DEBUG_MAKE_INDENT(3);
  alignment retval=ALIGN_UNSPEC;
  G_TRY
  {
    GP<GLObject> obj=parser.get_object(ALIGN_TAG);
    if (obj && obj->get_list().size()==2)
    {
      const GUTF8String align((*obj)[0]->get_symbol());
      DEBUG_MSG("hor_align='" << align << "'\n");
      
      for(int i=(int)ALIGN_UNSPEC;(i<align_strings_size);++i)
      {
        const alignment j=legal_halign(i);
        if((i == (int)j)&&(align == align_strings[i]))
        {
          retval=j;
          break;
        }
      }
    }
#ifndef NDEBUG
    if(retval == ALIGN_UNSPEC)
    {
      DEBUG_MSG("can't find any.\n");
    }
#endif // NDEBUG
  } G_CATCH_ALL {} G_ENDCATCH;
#ifndef NDEBUG
  if(retval == ALIGN_UNSPEC)
  {
    DEBUG_MSG("resetting alignment to ALIGN_UNSPEC\n");
  }
#endif // NDEBUG
  return retval;
}

DjVuANT::alignment
DjVuANT::get_ver_align(GLParser & parser)
{
  DEBUG_MSG("DjVuAnt::get_ver_align(): getting vert page alignemnt ...\n");
  DEBUG_MAKE_INDENT(3);
  alignment retval=ALIGN_UNSPEC;
  G_TRY
  {
    GP<GLObject> obj=parser.get_object(ALIGN_TAG);
    if (obj && obj->get_list().size()==2)
    {
      const GUTF8String align((*obj)[1]->get_symbol());
      DEBUG_MSG("ver_align='" << align << "'\n");
      for(int i=(int)ALIGN_UNSPEC;(i<align_strings_size);++i)
      {
        const alignment j=legal_valign(i);
        if((i == (int)j)&&(align == align_strings[i]))
        {
          retval=j;
          break;
        }
      }
    }
#ifndef NDEBUG
    if(retval == ALIGN_UNSPEC)
    {
      DEBUG_MSG("can't find any.\n");
    }
#endif // NDEBUG
  } G_CATCH_ALL {} G_ENDCATCH;
#ifndef NDEBUG
  if(retval == ALIGN_UNSPEC)
  {
    DEBUG_MSG("resetting alignment to ALIGN_UNSPEC\n");
  }
#endif // NDEBUG
  return retval;
}

GMap<GUTF8String, GUTF8String>
DjVuANT::get_metadata(GLParser & parser)
{
  DEBUG_MSG("DjVuANT::get_metadata(): forming and returning metadata table\n");
  DEBUG_MAKE_INDENT(3);
  
  GMap<GUTF8String, GUTF8String> mdata;
  
  GPList<GLObject> list=parser.get_list();
  for(GPosition pos=list;pos;++pos)
    {
      GLObject & obj=*list[pos];
      if (obj.get_type()==GLObject::LIST && obj.get_name()==METADATA_TAG)  
        { 
          G_TRY 
            {
              for(int obj_num=0;obj_num<obj.get_list().size();obj_num++)
                {
                  GLObject & el=*obj[obj_num];
                  const int type = el.get_type();
                  if (type == GLObject::LIST)
                    { 
                      const GUTF8String & name=el.get_name();  
                      mdata[name]=(el[0])->get_string();
                    }
                }
            } 
          G_CATCH_ALL { } G_ENDCATCH;
        }
    }
  return mdata;
}

GUTF8String
DjVuANT::get_xmpmetadata(GLParser & parser)
{
  DEBUG_MSG("DjVuANT::get_xmpmetadata(): returning xmp metadata string\n");
  DEBUG_MAKE_INDENT(3);
  
  GUTF8String xmp;
  GPList<GLObject> list=parser.get_list();
  for(GPosition pos=list;pos;++pos)
    {
      GLObject &obj = *list[pos];
      if (obj.get_type()==GLObject::LIST && obj.get_name()==XMP_TAG)  
        { 
          G_TRY 
            {
              if (obj.get_list().size() >= 1)
                {
                  GLObject &el = *obj[0];
                  xmp = el.get_string();
                  break;
                }
            } 
          G_CATCH_ALL { } G_ENDCATCH;
        }
    }
  return xmp;
}


GPList<GMapArea>
DjVuANT::get_map_areas(GLParser & parser)
{
  DEBUG_MSG("DjVuANT::get_map_areas(): forming and returning back list of map areas\n");
  DEBUG_MAKE_INDENT(3);
  
  GPList<GMapArea> map_areas;
  
  GPList<GLObject> list=parser.get_list();

  for(GPosition pos=list;pos;++pos)
  {
    GLObject & obj=*list[pos];
    const int type=obj.get_type();
    if (type == GLObject::LIST)
    {
      const GUTF8String name=obj.get_name();
      if(name == GMapArea::MAPAREA_TAG)
      {
        G_TRY {
	       // Getting the url
          GUTF8String url;
          GUTF8String target=GMapArea::TARGET_SELF;
          GLObject & url_obj=*(obj[0]);
          if (url_obj.get_type()==GLObject::LIST)
          {
            if (url_obj.get_name()!=GMapArea::URL_TAG)
              G_THROW( ERR_MSG("DjVuAnno.bad_url") );
            url=(url_obj[0])->get_string();
            target=(url_obj[1])->get_string();
          } else url=url_obj.get_string();
        
	       // Getting the comment
          GUTF8String comment=(obj[1])->get_string();
        
          DEBUG_MSG("found maparea '" << comment << "' (" <<
            url << ":" << target << ")\n");
        
          GLObject * shape=obj[2];
          GP<GMapArea> map_area;
          if (shape->get_type()==GLObject::LIST)
          {
            if (shape->get_name()==GMapArea::RECT_TAG)
            {
              DEBUG_MSG("it's a rectangle.\n");
              GRect grect((*shape)[0]->get_number(),
                          (*shape)[1]->get_number(),
                          (*shape)[2]->get_number(),
                          (*shape)[3]->get_number());
              GP<GMapRect> map_rect=GMapRect::create(grect);
              map_area=(GMapRect *)map_rect;
            } else if (shape->get_name()==GMapArea::POLY_TAG)
            {
              DEBUG_MSG("it's a polygon.\n");
              int points=shape->get_list().size()/2;
              GTArray<int> xx(points-1), yy(points-1);
              for(int i=0;i<points;i++)
              {
                xx[i]=(*shape)[2*i]->get_number();
                yy[i]=(*shape)[2*i+1]->get_number();
              }
              GP<GMapPoly> map_poly=GMapPoly::create(xx,yy,points);
              map_area=(GMapPoly *)map_poly;
            } else if (shape->get_name()==GMapArea::OVAL_TAG)
            {
              DEBUG_MSG("it's an ellipse.\n");
              GRect grect((*shape)[0]->get_number(),
                          (*shape)[1]->get_number(),
                          (*shape)[2]->get_number(),
                          (*shape)[3]->get_number());
              GP<GMapOval> map_oval=GMapOval::create(grect);
              map_area=(GMapOval *)map_oval;
            }
          }
        
          if (map_area)
          {
            map_area->url=url;
            map_area->target=target;
            map_area->comment=comment;
            for(int obj_num=3;obj_num<obj.get_list().size();obj_num++)
            {
              GLObject * el=obj[obj_num];
              if (el->get_type()==GLObject::LIST)
              {
                const GUTF8String & name=el->get_name();
                if (name==GMapArea::BORDER_AVIS_TAG)
                  map_area->border_always_visible=true;
                else if (name==GMapArea::HILITE_TAG)
                {
                  GLObject * obj=el->get_list()[el->get_list().firstpos()];
                  if (obj->get_type()==GLObject::SYMBOL)
                    map_area->hilite_color=cvt_color(obj->get_symbol(), 0xff);
                } else
                {
                  int border_type=
                    name==GMapArea::NO_BORDER_TAG ? GMapArea::NO_BORDER :
                    name==GMapArea::XOR_BORDER_TAG ? GMapArea::XOR_BORDER :
                    name==GMapArea::SOLID_BORDER_TAG ? GMapArea::SOLID_BORDER :
                    name==GMapArea::SHADOW_IN_BORDER_TAG ? GMapArea::SHADOW_IN_BORDER :
                    name==GMapArea::SHADOW_OUT_BORDER_TAG ? GMapArea::SHADOW_OUT_BORDER :
                    name==GMapArea::SHADOW_EIN_BORDER_TAG ? GMapArea::SHADOW_EIN_BORDER :
                    name==GMapArea::SHADOW_EOUT_BORDER_TAG ? GMapArea::SHADOW_EOUT_BORDER : -1;
                  if (border_type>=0)
                  {
                    map_area->border_type=(GMapArea::BorderType) border_type;
                    for(GPosition pos=el->get_list();pos;++pos)
                    {
                      GLObject * obj=el->get_list()[pos];
                      if (obj->get_type()==GLObject::SYMBOL)
                        map_area->border_color=cvt_color(obj->get_symbol(), 0xff);
                      if (obj->get_type()==GLObject::NUMBER)
                        map_area->border_width=obj->get_number();
                    }
                  }
                }	    
              } // if (el->get_type()==...)
            } // for(int obj_num=...)
            map_areas.append(map_area);
          } // if (map_area) ...
        } G_CATCH_ALL {} G_ENDCATCH;
      }
    }
  } // while(item==...)
   
  DEBUG_MSG("map area list size = " << list.size() << "\n");
  
  return map_areas;
}

void
DjVuANT::del_all_items(const char * name, GLParser & parser)
{
   GPList<GLObject> & list=parser.get_list();
   GPosition pos=list;
   while(pos)
   {
      GLObject & obj=*list[pos];
      if (obj.get_type()==GLObject::LIST &&
	  obj.get_name()==name)
      {
	 GPosition this_pos=pos;
	 ++pos;
	 list.del(this_pos);
      } else ++pos;
   }
}

GUTF8String
DjVuANT::encode_raw(void) const
{
   GUTF8String buffer;
   GLParser parser;

      //*** Background color
   del_all_items(BACKGROUND_TAG, parser);
   if (bg_color!=default_bg_color)
   {
      buffer.format("(" BACKGROUND_TAG " #%02X%02X%02X)",
	      (unsigned int)((bg_color & 0xff0000) >> 16),
	      (unsigned int)((bg_color & 0xff00) >> 8),
	      (unsigned int)(bg_color & 0xff));
      parser.parse(buffer);
   }

      //*** Zoom
   del_all_items(ZOOM_TAG, parser);
   if (zoom>0 || (zoom>=ZOOM_STRETCH && zoom<=ZOOM_PAGE))
   {
      buffer="(" ZOOM_TAG " ";
      if (zoom < 0)
        buffer += zoom_strings[-zoom];
      else
        buffer += "d"+GUTF8String(zoom);
      buffer+=")";
      parser.parse(buffer);
   }

      //*** Mode
   del_all_items(MODE_TAG, parser);
   if (mode!=MODE_UNSPEC)
   {
      const int i=mode-1;
      if((i>=0)&& (i<mode_strings_size))
      { 
        buffer="(" MODE_TAG " " + GUTF8String(mode_strings[mode]) + ")";
      }
      parser.parse(buffer);
   }

      //*** Alignment
   del_all_items(ALIGN_TAG, parser);
   if (hor_align!=ALIGN_UNSPEC || ver_align!=ALIGN_UNSPEC)
   {
      buffer= GUTF8String("(" ALIGN_TAG " ")
        +align_strings[((hor_align<ALIGN_UNSPEC)||
                        (hor_align>=align_strings_size))?ALIGN_UNSPEC:hor_align]
        +" "+align_strings[((ver_align<ALIGN_UNSPEC)||
                            (ver_align>=align_strings_size))?ALIGN_UNSPEC:ver_align]+")";
      parser.parse(buffer);
   }
      //*** Metadata
   del_all_items(METADATA_TAG, parser);
   if (!metadata.isempty())
     {
       GUTF8String mdatabuffer("(");
       mdatabuffer +=  METADATA_TAG ;
       for (GPosition pos=metadata; pos; ++pos)
         mdatabuffer +=" (" + metadata.key(pos) + " " + make_c_string(metadata[pos]) + ")";
       mdatabuffer += " )";
       parser.parse(mdatabuffer);
     }
      //*** XMP Metadata
   del_all_items(XMP_TAG, parser);
   if (!!xmpmetadata)
     {
       GUTF8String mdatabuffer("(");
       mdatabuffer +=  XMP_TAG;
       mdatabuffer += " " + make_c_string(xmpmetadata) + ")";
       parser.parse(mdatabuffer);
     }
     //*** Mapareas
   del_all_items(GMapArea::MAPAREA_TAG, parser);
   for(GPosition pos=map_areas;pos;++pos)
     {
       GUTF8String mapareabuffer = map_areas[pos]->print();
       parser.parse(mapareabuffer);
     }
   GP<ByteStream> gstr=ByteStream::create();
   ByteStream &str=*gstr;
   parser.print(str, 1);
   GUTF8String ans;
   int size = str.size();
   str.seek(0);
   str.read(ans.getbuf(size), size);
   return ans;
}

bool
DjVuANT::is_empty(void) const
{
   GUTF8String raw=encode_raw();
   for(int i=raw.length()-1;i>=0;i--)
     if (isspace((unsigned char)raw[i])) raw.setat(i, 0);
      else break;
   return raw.length()==0;
}

GP<DjVuANT>
DjVuANT::copy(void) const
{
   GP<DjVuANT> ant=new DjVuANT(*this);
      // Now process the list of hyperlinks.
   ant->map_areas.empty();
   for(GPosition pos=map_areas;pos;++pos)
      ant->map_areas.append(map_areas[pos]->get_copy());
   return ant;
}

//***************************************************************************
//******************************** DjVuAnno *********************************
//***************************************************************************

GUTF8String
DjVuAnno::get_xmlmap(const GUTF8String &name,const int height) const
{
  return ant
    ?(ant->get_xmlmap(name,height))
    :("<MAP name=\""+name.toEscaped()+"\"/>\n");
}

void
DjVuAnno::writeMap(ByteStream &str_out,const GUTF8String &name,const int height) const
{
  if(ant)
  {
    ant->writeMap(str_out,name,height);
  }else
  {
    str_out.writestring(get_xmlmap(name,height));
  }
}

GUTF8String
DjVuAnno::get_paramtags(void) const
{
  return ant
    ?(ant->get_paramtags())
    :GUTF8String();
}

void
DjVuAnno::writeParam(ByteStream &str_out) const
{
  str_out.writestring(get_paramtags());
}


void
DjVuAnno::decode(const GP<ByteStream> &gbs)
{
  GUTF8String chkid;
  GP<IFFByteStream> giff=IFFByteStream::create(gbs);
  IFFByteStream &iff=*giff;
  while( iff.get_chunk(chkid) )
  {
    if (chkid == "ANTa")
    {
      if (ant) {
        ant->merge(*iff.get_bytestream());
      } else {
        ant=DjVuANT::create();
        ant->decode(*iff.get_bytestream());
      }
    }
    else if (chkid == "ANTz")
    {
      GP<ByteStream> gbsiff=BSByteStream::create(giff->get_bytestream());
      if (ant) {
        ant->merge(*gbsiff);
      } else {
        ant=DjVuANT::create();
        ant->decode(*gbsiff);
      }
    }
    // Add decoding of other chunks here
    iff.close_chunk();
  }
}

void
DjVuAnno::encode(const GP<ByteStream> &gbs)
{
  GP<IFFByteStream> giff=IFFByteStream::create(gbs);
  IFFByteStream &iff=*giff;
  if (ant)
    {
#if 0
      iff.put_chunk("ANTa");
      ant->encode(iff);
      iff.close_chunk();
#else
      iff.put_chunk("ANTz");
      {
//	 GP<ByteStream> bsbinput = giff.get_bytestream();
	 GP<ByteStream> bsb = BSByteStream::create(giff->get_bytestream(), 50);
	 ant->encode(*bsb);
      }
      iff.close_chunk();
#endif
    }
  // Add encoding of other chunks here
}


GP<DjVuAnno>
DjVuAnno::copy(void) const
{
   GP<DjVuAnno> anno= new DjVuAnno;
      // Copy any primitives (if any)
   *anno=*this;
      // Copy each substructure
   if (ant) anno->ant = ant->copy();
   return anno;
}

void
DjVuAnno::merge(const GP<DjVuAnno> & anno)
{
   if (anno)
   {
      GP<ByteStream> gstr=ByteStream::create();
      encode(gstr);
      anno->encode(gstr);
      gstr->seek(0);
      decode(gstr);
   }
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
