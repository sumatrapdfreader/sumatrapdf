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
// All these XML messages are Lizardtech innovations.

#include "DjVuMessage.h"
#include "GOS.h"
#include "XMLTags.h"
#include "ByteStream.h"
#include "GURL.h"
#include "debug.h"
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#ifdef _WIN32
# include <tchar.h>
# include <windows.h>
# include <winreg.h>
#endif
#ifdef UNIX
# include <unistd.h>
# include <pwd.h>
# include <sys/types.h>
#endif
#include <locale.h>
#ifndef LC_MESSAGES
# define LC_MESSAGES LC_ALL
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

GUTF8String &
DjVuMessage::programname(void)
{
  static GUTF8String xprogramname;
  use_language();
  return xprogramname;
}

static const char namestring[]="name";
static const char srcstring[]="src";

static const char *failed_to_parse_XML=ERR_MSG("DjVuMessage.failed_to_parse_XML");
static const char bodystring[]="BODY";
static const char languagestring[]="LANGUAGE";
static const char headstring[]="HEAD";
static const char includestring[]="INCLUDE";
static const char messagestring[]="MESSAGE";
static const char localestring[]="locale";


// directory names for searching messages
#ifdef AUTOCONF
static const char DjVuDataDir[] = DIR_DATADIR "/djvu/osi";
#endif /* AUTOCONF */
static const char ModuleDjVuDir[] ="share/djvu/osi";
static const char ProfilesDjVuDir[] ="profiles";
static const char LocalDjVuDir[] =".DjVu";      // relative to ${HOME}
#ifdef LT_DEFAULT_PREFIX
static const char DjVuPrefixDir[] = LT_DEFAULT_PREFIX "/profiles";
#endif
#ifndef NDEBUG
static const char DebugModuleDjVuDir[] ="../TOPDIR/SRCDIR/profiles";
#endif
#ifdef _WIN32
static const char RootDjVuDir[] ="C:/Program Files/LizardTech/Profiles";
static const TCHAR registrypath[]= TEXT("Software\\LizardTech\\DjVu\\Profile Path");
#else
static const char RootDjVuDir[] ="/etc/DjVu/";  // global last resort
#endif

static const char DjVuEnv[] = "DJVU_CONFIG_DIR";

//  The name of the message file
static const char MessageFile[]="messages.xml";
static const char LanguageFile[]="languages.xml";

#ifdef _WIN32
static GURL
RegOpenReadConfig ( HKEY hParentKey )
{
  GURL retval;
  LPCTSTR path = registrypath;
  HKEY hKey = 0;
  if (RegOpenKeyEx(hParentKey, path, 0,
		   KEY_READ, &hKey) == ERROR_SUCCESS )
  {
    CHAR path[1024];
    CHAR *szPathValue = path;
    LPCSTR lpszEntry = "";
    DWORD dwCount = (sizeof(path)/sizeof(CHAR))-1;
    DWORD dwType;
    LONG lResult = RegQueryValueExA(hKey, lpszEntry, NULL,
			&dwType, (LPBYTE)szPathValue, &dwCount);
    RegCloseKey(hKey);
    if ((lResult == ERROR_SUCCESS))
    {
      szPathValue[dwCount] = 0;
      retval=GURL::Filename::Native(path);
    }
  } 
  return retval;
}

static GURL
GetModulePath( void )
{
  const GUTF8String cwd(GOS::cwd());
  CHAR path[1024];
  DWORD dwCount = (sizeof(path)/sizeof(CHAR))-1;
  GetModuleFileNameA(0, path, dwCount);
  GURL retval=GURL::Filename::Native(path).base();
  GOS::cwd(cwd);
  return retval;
}
#elif defined(UNIX)

static GList<GURL>
parsePATH(void)
{
  GList<GURL> retval;
  const char *path=getenv("PATH");
  if(path)
  {
    GNativeString p(path);
    int from=0;
    for(int to;(to=p.search(':',from))>0;from=to+1)
    {
      if(to > from)
      {
        retval.append(GURL::Filename::Native(p.substr(from,to-from)));
      }
    }
    if((from+1)<(int)p.length())
    {
      retval.append(GURL::Filename::Native(p.substr(from,-1)));
    }
  }
  return retval;
}

static GURL
GetModulePath( void )
{
 GURL retval;
 GUTF8String &xprogramname=DjVuMessage::programname();
 if(xprogramname.length())
 {
   if(xprogramname[1]=='/'
     ||!xprogramname.cmp("../",3)
     ||!xprogramname.cmp("./",2))
   {
     retval=GURL::Filename::UTF8(xprogramname);
   }
   if(retval.is_empty() || !retval.is_file())
   {
     GList<GURL> paths(parsePATH());
     GMap<GUTF8String,void *> pathMAP;
     for(GPosition pos=paths;pos;++pos)
     {
       retval=GURL::UTF8(xprogramname,paths[pos]);
       const GUTF8String path(retval.get_string());
       if(!pathMAP.contains(path))
       {
         if(retval.is_file())
           break;
         pathMAP[path]=0;
       }
     }
   }
   if (! retval.is_empty() )
     retval = retval.follow_symlinks();
   if (! retval.is_empty() )
     retval = retval.base();
 }
 return retval;
}
#endif

static void
appendPath(const GURL &url, 
           GMap<GUTF8String,void *> &map,
           GList<GURL> &list)
{
  if( !url.is_empty() && !map.contains(url.get_string()) )
    {
      map[url.get_string()]=0;
      list.append(url);
    }
}

GList<GURL>
DjVuMessage::GetProfilePaths(void)
{
  static bool first=true;
  static GList<GURL> realpaths;
  if(first)
  {
    first=false;
    GMap<GUTF8String,void *> pathsmap;
    GList<GURL> paths;
    GURL path;
    const GUTF8String envp(GOS::getenv(DjVuEnv));
    if(envp.length())
      appendPath(GURL::Filename::UTF8(envp),pathsmap,paths);
#if defined(_WIN32) || defined(UNIX)
    GURL mpath(GetModulePath());
    if(!mpath.is_empty() && mpath.is_dir())
    {
#if defined(UNIX) && !defined(AUTOCONF) && !defined(NDEBUG)
      appendPath(GURL::UTF8(DebugModuleDjVuDir,mpath),pathsmap,paths);
#endif
      appendPath(mpath,pathsmap,paths);
      appendPath(GURL::UTF8(ModuleDjVuDir,mpath),pathsmap,paths);
      appendPath(GURL::UTF8(ProfilesDjVuDir,mpath),pathsmap,paths);
      mpath=mpath.base();
      appendPath(GURL::UTF8(ModuleDjVuDir,mpath),pathsmap,paths);
      appendPath(GURL::UTF8(ProfilesDjVuDir,mpath),pathsmap,paths);
      mpath=mpath.base();
      appendPath(GURL::UTF8(ModuleDjVuDir,mpath),pathsmap,paths);
      appendPath(GURL::UTF8(ProfilesDjVuDir,mpath),pathsmap,paths);
      mpath=mpath.base();
      appendPath(GURL::UTF8(ModuleDjVuDir,mpath),pathsmap,paths);
      appendPath(GURL::UTF8(ProfilesDjVuDir,mpath),pathsmap,paths);
    }
#endif
#if defined(AUTOCONF)
    GURL dpath = GURL::Filename::UTF8(DjVuDataDir);
    appendPath(dpath,pathsmap,paths);
#endif
#ifdef _WIN32
    appendPath(RegOpenReadConfig(HKEY_CURRENT_USER),pathsmap,paths);
    appendPath(RegOpenReadConfig(HKEY_LOCAL_MACHINE),pathsmap,paths);
#else
    GUTF8String home=GOS::getenv("HOME");
# if HAVE_GETPWUID
    if (! home.length()) {
      struct passwd *pw=0;
      if ((pw = getpwuid(getuid())))
        home=GNativeString(pw->pw_dir);
    }
# endif
    if (home.length()) {
      GURL hpath = GURL::UTF8(LocalDjVuDir,GURL::Filename::UTF8(home));
      appendPath(hpath,pathsmap,paths);
    }
#endif
#ifdef LT_DEFAULT_PREFIX
    appendPath(GURL::Filename::UTF8(DjVuPrefixDir),pathsmap,paths);
#endif
    appendPath(GURL::Filename::UTF8(RootDjVuDir),pathsmap,paths);
    pathsmap.empty();

    GPosition pos;
    GList< GMap<GUTF8String,GP<lt_XMLTags> > > localemaps;
    for(pos=paths;pos;++pos)
    {
      path=GURL::UTF8(LanguageFile,paths[pos]);
      if(path.is_file())
      {
        const GP<lt_XMLTags> xml(lt_XMLTags::create(ByteStream::create(path,"rb")));
        const GPList<lt_XMLTags> Body(xml->get_Tags(bodystring));
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
        GMap<GUTF8String,GP<lt_XMLTags> > localemap;
        lt_XMLTags::get_Maps(languagestring,localestring,Body,localemap);
        localemaps.append(localemap);
      }
    } 
    GList<GURL> localepaths;

    // Need to do it the right way!
    GUTF8String defaultlocale = getenv("LANGUAGE");
    if (! defaultlocale) 
      {
        const GUTF8String oldlocale(setlocale(LC_MESSAGES,0));
        defaultlocale = setlocale(LC_MESSAGES,"");
        setlocale(LC_MESSAGES,(const char *)oldlocale);
      }
    // Unfathomable search.
    for(int loop=0; loop<2; loop++)
    {
      static const char sepchars[]=" _.@";
      const char *p=sepchars+sizeof(sepchars)-1;
      do
      {
        int sepcharpos=p[0]?defaultlocale.search(p[0]):defaultlocale.length();
        if(sepcharpos > 0)
        {
          const GUTF8String sublocale(defaultlocale,sepcharpos);
          const GUTF8String downcasesublocale("downcase^"+sublocale.downcase());
          for(pos=localemaps;pos;++pos) 
          {
            const GMap<GUTF8String,GP<lt_XMLTags> > &localemap=localemaps[pos];
            GPosition pos=localemap.contains(sublocale);
            if(!pos)
              pos=localemap.contains(downcasesublocale);
            if(pos)
            {
              const GMap<GUTF8String,GUTF8String>&args
                = localemap[pos]->get_args();
              pos = args.contains(srcstring);
              if (pos)
              {
                const GUTF8String src(args[pos]);
                for(pos=paths;pos;++pos)
                {
                  path=GURL::UTF8(src,paths[pos]);
                  if(path.is_dir())
                    localepaths.append(path);
                }
              }
              // We don't need to check anymore language files.
              p=sepchars;
              break;
            }
          }
          if(!pos)
            {
            for(pos=paths;pos;++pos)
              {
              path=GURL::UTF8(sublocale,paths[pos]);
              if(path.is_dir())
                localepaths.append(path);
            }
          }
        }
      } while(p-- != sepchars);
      if((GPosition) localepaths)
        break;
      defaultlocale="C";
    }
    for(pos=localepaths;pos;++pos)
      appendPath(localepaths[pos],pathsmap,realpaths);
    for(pos=paths;pos;++pos)
      appendPath(paths[pos],pathsmap,realpaths);
  }
  return realpaths;
}

static GUTF8String
getbodies(
  GList<GURL> &paths,
  const GUTF8String &MessageFileName,
  GPList<lt_XMLTags> &body, 
  GMap<GUTF8String, void *> & map )
{
  GUTF8String errors;
  bool isdone=false;
  GPosition firstpathpos=paths;
  for(GPosition pathpos=firstpathpos;!isdone && pathpos;++pathpos)
  {
    const GURL::UTF8 url(MessageFileName,paths[pathpos]);
    if(url.is_file())
    {
      map[MessageFileName]=0;
      GP<lt_XMLTags> gtags;
      {
        GP<ByteStream> bs=ByteStream::create(url,"rb");
        G_TRY
        {
          gtags=lt_XMLTags::create(bs);
        }
        G_CATCH(ex)
        {
          GUTF8String mesg(failed_to_parse_XML+("\t"+url.get_string()));
          if(errors.length())
          {
            errors+="\n"+mesg;
          }else
          {
            errors=mesg;
          }
          errors+="\n"+GUTF8String(ex.get_cause());
        }
        G_ENDCATCH;
      }
      if(gtags)
      {
        lt_XMLTags &tags=*gtags;
        GPList<lt_XMLTags> Bodies=tags.get_Tags(bodystring);
        if(! Bodies.isempty())
        {
          isdone=true;
          for(GPosition pos=Bodies;pos;++pos)
          {
            body.append(Bodies[pos]);
          }
        }
        GPList<lt_XMLTags> Head=tags.get_Tags(headstring);
        if(! Head.isempty())
        {
          isdone=true;
          GMap<GUTF8String, GP<lt_XMLTags> > includes;
          lt_XMLTags::get_Maps(includestring,namestring,Head,includes);
          for(GPosition pos=includes;pos;++pos)
          {
            const GUTF8String file=includes.key(pos);
            if(! map.contains(file))
            {
              GList<GURL> xpaths;
              xpaths.append(url.base());
              const GUTF8String err2(getbodies(xpaths,file,body,map));
              if(err2.length())
              {
                if(errors.length())
                {
                  errors+="\n"+err2;
                }else
                {
                  errors=err2;
                }
              }
            }
          }
        }
      }
    }
  }
  return errors;
}

static GUTF8String
parse(GMap<GUTF8String,GP<lt_XMLTags> > &retval)
{
  GUTF8String errors;
  GPList<lt_XMLTags> body;
  if (0) /* SumatraPDF: don't bother looking for messages.xml and languages.xml using broken code */
  {
    GList<GURL> paths=DjVuMessage::GetProfilePaths();
    GMap<GUTF8String, void *> map;
    GUTF8String m(MessageFile);
    errors=getbodies(paths,m,body,map);
  }
  if(! body.isempty())
  {
    lt_XMLTags::get_Maps(messagestring,namestring,body,retval);
  }
  return errors;
}


const DjVuMessageLite &
DjVuMessage::create_full(void)
{
  GP<DjVuMessageLite> &static_message=getDjVuMessageLite();
  if(!static_message)
  {
    DjVuMessage *mesg=new DjVuMessage;
    static_message=mesg;
    mesg->init();
  }
  return DjVuMessageLite::create_lite();
}

void
DjVuMessage::set_programname(const GUTF8String &xprogramname)
{
  programname()=xprogramname;
  DjVuMessageLite::create=create_full; 
}

void
DjVuMessage::use_language(void)
{ 
  DjVuMessageLite::create=create_full; 
}


// Constructor
DjVuMessage::DjVuMessage( void ) {}

void
DjVuMessage::init(void)
{
  errors=parse(Map);
}

// Destructor
DjVuMessage::~DjVuMessage( )
{
}


//  A C function to perform a message lookup. Arguments are a buffer to receiv
//  translated message, a buffer size (bytes), and a message_list. The transla
//  result is returned in msg_buffer encoded in Native MBS encoding. In case
// of error, msg_b empty (i.e., msg_buffer[0] == '\0').
void
DjVuMessageLookUpNative( 
  char *msg_buffer, const unsigned int buffer_size, const char *message)
{
  const GNativeString converted(DjVuMessage::LookUpNative( message ));
  if( converted.length() >= buffer_size )
    msg_buffer[0] = '\0';
  else
    strcpy( msg_buffer, converted );
}

//  A C function to perform a message lookup. Arguments are a buffer to receiv
//  translated message, a buffer size (bytes), and a message_list. The transla
//  result is returned in msg_buffer encoded in UTF8 encoding. In case
// of error, msg_b empty (i.e., msg_buffer[0] == '\0').
void
DjVuMessageLookUpUTF8( 
  char *msg_buffer, const unsigned int buffer_size, const char *message)
{
  const GUTF8String converted(DjVuMessage::LookUpUTF8( message ));
  if( converted.length() >= buffer_size )
    msg_buffer[0] = '\0';
  else
    strcpy( msg_buffer, converted );
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

void
DjVuFormatErrorUTF8( const char *fmt, ... )
{
  va_list args;
  va_start(args, fmt); 
  const GUTF8String message(fmt,args);
  DjVuWriteError( message );
}

void
DjVuFormatErrorNative( const char *fmt, ... )
{
  va_list args;
  va_start(args, fmt); 
  const GNativeString message(fmt,args);
  DjVuWriteError( message );
}

const char *
djvu_programname(const char *xprogramname)
{
  if(xprogramname)
    DjVuMessage::programname()=GNativeString(xprogramname);
  return DjVuMessage::programname();
}
