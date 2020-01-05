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
// This has been heavily changed by Lizardtech.
// They decided to use URLs for everyting, including
// the most basic file access.  The URL class now is a unholy 
// mixture of code for syntactically parsing the urls (which it was)
// and file status code (only for local file: urls).

#include "GException.h"
#include "GOS.h"
#include "GURL.h"
#include "debug.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#ifdef _WIN32
# include <tchar.h>
# include <windows.h>
# include <direct.h>
#endif /* WIN32 */

// -- MAXPATHLEN
#ifndef MAXPATHLEN
# ifdef _MAX_PATH
#  define MAXPATHLEN _MAX_PATH
# else
#  define MAXPATHLEN 1024
# endif
#else
# if ( MAXPATHLEN < 1024 )
#  undef MAXPATHLEN
#  define MAXPATHLEN 1024
# endif
#endif

#if defined(UNIX) || defined(OS2)
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <errno.h>
# include <fcntl.h>
# include <pwd.h>
# include <stdio.h>
# ifdef AUTOCONF
#  ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#  else
#   ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#   else
#    include <time.h>
#   endif
#  endif
#  ifdef HAVE_DIRENT_H
#   include <dirent.h>
#   define NAMLEN(dirent) strlen((dirent)->d_name)
#  else
#   define dirent direct
#   define NAMLEN(dirent) (dirent)->d_namlen
#   ifdef HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#   endif
#   ifdef HAVE_SYS_DIR_H
#    include <sys/dir.h>
#   endif
#   ifdef HAVE_NDIR_H
#    include <ndir.h>
#   endif
#  endif
# else /* !AUTOCONF */ 
#  include <sys/time.h>
#  if defined(XENIX)
#   define USE_DIRECT
#   include <sys/ndir.h>
#  elif defined(OLDBSD)
#   define USE_DIRECT
#   include <sys/dir.h>
#  endif
#  ifdef USE_DIRECT
#   define dirent direct
#   define NAMLEN(dirent) (dirent)->d_namlen
#  else
#   include <dirent.h>
#   define NAMLEN(dirent) strlen((dirent)->d_name)
#  endif 
# endif /* !AUTOCONF */
#endif /* UNIX */

#ifdef macintosh
#include <unix.h>
#include <errno.h>
#include <unistd.h>
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


static const char djvuopts[]="DJVUOPTS";
static const char localhost[]="file://localhost/";
static const char colon=':';
static const char dot='.';
static const char filespecslashes[] = "file://";
static const char filespec[] = "file:";
static const char slash='/';
static const char percent='%';
static const char localhostspec1[] = "//localhost/";
static const char localhostspec2[] = "///";
#if defined(UNIX)
  static const char tilde='~';
  static const char root[] = "/";
#elif defined(_WIN32) || defined(OS2)
  static const char root[] = "\\";
  static const char backslash='\\';  
#elif defined(macintosh)
  static const char nillchar=0;
  static char const * const root = &nillchar; 
#else
#error "Define something here for your operating system"
#endif


static const int
pathname_start(const GUTF8String &url, const int protolength);

// hexval --
// -- Returns the hexvalue of a character.
//    Returns -1 if illegal;

static int 
hexval(char c)
{
  return ((c>='0' && c<='9')
    ?(c-'0')
    :((c>='A' && c<='F')
      ?(c-'A'+10)
      :((c>='a' && c<='f')
        ?(c-'a'+10):(-1))));
}


static bool
is_argument(const char * start)
      // Returns TRUE if 'start' points to the beginning of an argument
      // (either hash or CGI)
{
   // return (*start=='#' || *start=='?' || *start=='&' || *start==';');
   return (*start=='#' || *start=='?' );
}

static bool
is_argument_sep(const char * start)
      // Returns TRUE if 'start' points to the beginning of an argument
      // (either hash or CGI)
{
   return (*start=='&')||(*start == ';');
}

void
GURL::convert_slashes(void)
{
   GUTF8String xurl(get_string());
#if defined(_WIN32)
   const int protocol_length=protocol(xurl).length();
   for(char *ptr=(xurl.getbuf()+protocol_length);*ptr;ptr++)
     if(*ptr == backslash)
       *ptr=slash;
   url=xurl;
#endif
}

static void
collapse(char * ptr, const int chars)
      // Will remove the first 'chars' chars from the string and
      // move the rest toward the beginning. Will take into account
      // string length
{
   const int length=strlen(ptr);
   const char *srcptr=ptr+((chars>length)?length:chars);
   while((*(ptr++) = *(srcptr++)))
     EMPTY_LOOP;
}

GUTF8String
GURL::beautify_path(GUTF8String xurl)
{

  const int protocol_length=GURL::protocol(xurl).length();
   
  // Eats parts like ./ or ../ or ///
  char * buffer;
  GPBuffer<char> gbuffer(buffer,xurl.length()+1);
  strcpy(buffer, (const char *)xurl);
   
  // Find start point
  char * start=buffer+pathname_start(xurl,protocol_length);

  // Find end of the url (don't touch arguments)
  char * ptr;
  GUTF8String args;
  for(ptr=start;*ptr;ptr++)
  {
    if (is_argument(ptr))
    {
      args=ptr;
      *ptr=0;
      break;
    }
  }

  // Eat multiple slashes
  for(;(ptr=strstr(start, "////"));collapse(ptr, 3))
    EMPTY_LOOP;
  for(;(ptr=strstr(start, "//"));collapse(ptr, 1))
    EMPTY_LOOP;
  // Convert /./ stuff into plain /
  for(;(ptr=strstr(start, "/./"));collapse(ptr, 2))
    EMPTY_LOOP;
#if defined(_WIN32) || defined(OS2)
  if(!xurl.cmp(filespec,sizeof(filespec)-1))
  {
	int offset=1;
	if(start&&(start[0] == '/')&& 
           !xurl.cmp("file:////",sizeof("file:////")-1))
	{
	  collapse(start, 1);
	  offset=0;
	}
    for(ptr=start+offset;(ptr=strchr(ptr, '/'));)
	{
	  if(isalpha((unsigned char)((++ptr)[0])))
	  {
	    if((ptr[1] == ':')&&(ptr[2]=='/'))
		{
		  char *buffer2;
                  GPBuffer<char> gbuffer2(buffer2,strlen(ptr)+1);
		  strcpy(buffer2,ptr);
		  gbuffer.resize(strlen(ptr)+sizeof(localhost));
		  strcpy(buffer,localhost);
		  strcat(buffer,buffer2);
		  ptr=(start=buffer+sizeof(localhost))+1;
		}
	  }
	}
  }
#endif
  // Process /../
  while((ptr=strstr(start, "/../")))
  {
    for(char * ptr1=ptr-1;(ptr1>=start);ptr1--)
    {
      if (*ptr1==slash)
      {
        collapse(ptr1, ptr-ptr1+3);
        break;
      }
    }
  }

  // Remove trailing /.
  ptr=start+strlen(start)-2;
  if((ptr>=start)&& (ptr == GUTF8String("/.")))
  {
    ptr[1]=0;
  }
  // Eat trailing /..
  ptr=start+strlen(start)-3;
  if((ptr >= start) && (ptr == GUTF8String("/..")))
  {
    for(char * ptr1=ptr-1;(ptr1>=start);ptr1--)
    {
      if (*ptr1==slash)
      {
        ptr1[1]=0;
        break;
      }
    }
  }

  // Done. Copy the buffer back into the URL and add arguments.
  xurl=buffer;
  return (xurl+args);
}


void
GURL::beautify_path(void)
{
  url=beautify_path(get_string());
}

void
GURL::init(const bool nothrow)
{
   GCriticalSectionLock lock(&class_lock);
   validurl=true;
   
   if (url.length())
   {
      GUTF8String proto=protocol();
      if (proto.length()<2)
      {
        validurl=false;
        if(!nothrow)
          G_THROW( ERR_MSG("GURL.no_protocol") "\t"+url);
        return;
      }

         // Below we have to make this complex test to detect URLs really
         // referring to *local* files. Surprisingly, file://hostname/dir/file
         // is also valid, but shouldn't be treated thru local FS.
      if (proto=="file" && url[5]==slash &&
          (url[6]!=slash || !url.cmp(localhost, sizeof(localhost))))
      {
            // Separate the arguments
         GUTF8String arg;
         {
           const char * const url_ptr=url;
           const char * ptr;
           for(ptr=url_ptr;*ptr&&!is_argument(ptr);ptr++)
           		EMPTY_LOOP;
           arg=ptr;
           url=url.substr(0,(size_t)(ptr-url_ptr));
         }

            // Do double conversion
         GUTF8String tmp=UTF8Filename();
         if (!tmp.length())
         {
           validurl=false;
           if(!nothrow)
             G_THROW( ERR_MSG("GURL.fail_to_file") );
           return;
         }
         url=GURL::Filename::UTF8(tmp).get_string();
         if (!url.length())
         {
           validurl=false;
           if(!nothrow)
             G_THROW( ERR_MSG("GURL.fail_to_URL") );
           return;
         }
            // Return the argument back
         url+=arg;
      }
      convert_slashes();
      beautify_path();
      parse_cgi_args();
   }
}

GURL::GURL(void) 
  : validurl(false) 
{
}

GURL::GURL(const char * url_in) 
  : url(url_in ? url_in : ""), validurl(false)
{
}

GURL::GURL(const GUTF8String & url_in)
  : url(url_in), validurl(false)
{
}

GURL::GURL(const GNativeString & url_in)
  : url(url_in.getNative2UTF8()), validurl(false)
{
#if defined(_WIN32) || defined(OS2)
  if(is_valid() && is_local_file_url())
  {
    GURL::Filename::UTF8 xurl(UTF8Filename());
    url=xurl.get_string(true);
    validurl=false;
  }
#endif
}

GURL::GURL(const GURL & url_in)
  : validurl(false)
{
  if(url_in.is_valid())
  {
    url=url_in.get_string();
    init();
  }else
  {
    url=url_in.url;
  }
}

GURL &
GURL::operator=(const GURL & url_in)
{
   GCriticalSectionLock lock(&class_lock);
   if(url_in.is_valid())
   {
     url=url_in.get_string();
     init(true);
   }else
   {
     url=url_in.url;
     validurl=false;
   }
   return *this;
}

GUTF8String
GURL::protocol(const GUTF8String& url)
{
  const char * const url_ptr=url;
  const char * ptr=url_ptr;
  for(char c=*ptr;
      c && isascii(c) && (isalnum(c) || c == '+' || c == '-' || c == '.');
      c=*(++ptr)) EMPTY_LOOP;
  if (ptr[0]==colon && ptr[1]=='/' && ptr[2]=='/')
    return GUTF8String(url_ptr, ptr-url_ptr);
  return GUTF8String();
}

GUTF8String
GURL::hash_argument(void) const
      // Returns the HASH argument (anything after '#' and before '?')
{
   const GUTF8String xurl(get_string());

   bool found=false;
   GUTF8String arg;

         // Break if CGI argument is found
   for(const char * start=xurl;*start&&(*start!='?');start++)
   {
      if (found)
      {
         arg+=*start;
      }else
      {
         found=(*start=='#');
      }
   }
   return decode_reserved(arg);
}

void
GURL::set_hash_argument(const GUTF8String &arg)
{
   const GUTF8String xurl(get_string());

   GUTF8String new_url;
   bool found=false;
   const char * ptr;
   for(ptr=xurl;*ptr;ptr++)
   {
      if (is_argument(ptr))
      {
         if (*ptr!='#')
         {
           break;
         }
         found=true;
      } else if (!found)
      {
         new_url+=*ptr;
      }
   }

   url=new_url+"#"+GURL::encode_reserved(arg)+ptr;
}

void
GURL::parse_cgi_args(void)
      // Will read CGI arguments from the URL into
      // cgi_name_arr and cgi_value_arr
{
   if(!validurl)
     init();
   GCriticalSectionLock lock1(&class_lock);
   cgi_name_arr.empty();
   cgi_value_arr.empty();

      // Search for the beginning of CGI arguments
   const char * start=url;
   while(*start)
   {
     if(*(start++)=='?')
     {
       break;
     }
   }

      // Now loop until we see all of them
   while(*start)
   {
      GUTF8String arg;        // Storage for another argument
      while(*start)        // Seek for the end of it
      {
         if (is_argument_sep(start))
         {
            start++;
            break;
         } else
         {
           arg+=*start++;
         }
      }
      if (arg.length())
      {
            // Got argument in 'arg'. Split it into 'name' and 'value'
         const char * ptr;
         const char * const arg_ptr=arg;
	 for(ptr=arg_ptr;*ptr&&(*ptr != '=');ptr++)
	   EMPTY_LOOP;

         GUTF8String name, value;
         if (*ptr)
         {
            name=GUTF8String(arg_ptr, (int)((ptr++)-arg_ptr));
            value=GUTF8String(ptr, arg.length()-name.length()-1);
         } else
         {
           name=arg;
         }
            
         int args=cgi_name_arr.size();
         cgi_name_arr.resize(args);
         cgi_value_arr.resize(args);
         cgi_name_arr[args]=decode_reserved(name);
         cgi_value_arr[args]=decode_reserved(value);
      }
   }
}

void
GURL::store_cgi_args(void)
      // Will store CGI arguments from the cgi_name_arr and cgi_value_arr
      // back into the URL
{
   if(!validurl)
     init();
   GCriticalSectionLock lock1(&class_lock);

   const char * const url_ptr=url;
   const char * ptr;
   for(ptr=url_ptr;*ptr&&(*ptr!='?');ptr++)
   		EMPTY_LOOP;
   
   GUTF8String new_url(url_ptr, ptr-url_ptr);
   
   for(int i=0;i<cgi_name_arr.size();i++)
   {
      GUTF8String name=GURL::encode_reserved(cgi_name_arr[i]);
      GUTF8String value=GURL::encode_reserved(cgi_value_arr[i]);
      new_url+=(i?"&":"?")+name;
      if (value.length())
         new_url+="="+value;
   }

   url=new_url;
}

int
GURL::cgi_arguments(void) const
{
   if(!validurl)
      const_cast<GURL *>(this)->init();
   return cgi_name_arr.size();
}

int
GURL::djvu_cgi_arguments(void) const
{
   if(!validurl)
     const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   int args=0;
   for(int i=0;i<cgi_name_arr.size();i++)
   {
      if (cgi_name_arr[i].upcase()==djvuopts)
      {
         args=cgi_name_arr.size()-(i+1);
         break;
      }
   } 
   return args;
}

GUTF8String
GURL::cgi_name(int num) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   return (num<cgi_name_arr.size())?cgi_name_arr[num]:GUTF8String();
}

GUTF8String
GURL::djvu_cgi_name(int num) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   GUTF8String arg;
   for(int i=0;i<cgi_name_arr.size();i++)
      if (cgi_name_arr[i].upcase()==djvuopts)
      {
         for(i++;i<cgi_name_arr.size();i++)
            if (! num--)
            {
               arg=cgi_name_arr[i];
               break;
            }
         break;
      }
   return arg;
}

GUTF8String
GURL::cgi_value(int num) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   return (num<cgi_value_arr.size())?cgi_value_arr[num]:GUTF8String();
}

GUTF8String
GURL::djvu_cgi_value(int num) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   GUTF8String arg;
   for(int i=0;i<cgi_name_arr.size();i++)
   {
      if (cgi_name_arr[i].upcase()==djvuopts)
      {
         for(i++;i<cgi_name_arr.size();i++)
         {
            if (! num--)
            {
               arg=cgi_value_arr[i];
               break;
            }
         }
         break;
      }
   }
   return arg;
}

DArray<GUTF8String>
GURL::cgi_names(void) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   return cgi_name_arr;
}

DArray<GUTF8String>
GURL::cgi_values(void) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   return cgi_value_arr;
}

DArray<GUTF8String>
GURL::djvu_cgi_names(void) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   int i;
   DArray<GUTF8String> arr;
   for(i=0;(i<cgi_name_arr.size())&&
     (cgi_name_arr[i].upcase()!=djvuopts)
     ;i++)
     	EMPTY_LOOP;

   int size=cgi_name_arr.size()-(i+1);
   if (size>0)
   {
      arr.resize(size-1);
      for(i=0;i<arr.size();i++)
         arr[i]=cgi_name_arr[cgi_name_arr.size()-arr.size()+i];
   }

   return arr;
}

DArray<GUTF8String>
GURL::djvu_cgi_values(void) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);

   int i;
   DArray<GUTF8String> arr;
   for(i=0;i<cgi_name_arr.size()&&(cgi_name_arr[i].upcase()!=djvuopts);i++)
   		EMPTY_LOOP;

   int size=cgi_name_arr.size()-(i+1);
   if (size>0)
   {
      arr.resize(size-1);
      for(i=0;i<arr.size();i++)
         arr[i]=cgi_value_arr[cgi_value_arr.size()-arr.size()+i];
   }

   return arr;
}

void
GURL::clear_all_arguments(void)
{
   clear_hash_argument();
   clear_cgi_arguments();
}

void
GURL::clear_hash_argument(void)
      // Clear anything after first '#' and before the following '?'
{
   if(!validurl) init();
   GCriticalSectionLock lock(&class_lock);
   bool found=false;
   GUTF8String new_url;
   for(const char * start=url;*start;start++)
   {
         // Break on first CGI arg.
      if (*start=='?')
      {
         new_url+=start;
         break;
      }

      if (!found)
      { 
        if (*start=='#')
          found=true;
        else
          new_url+=*start;
      }
   }
   url=new_url;
}

void
GURL::clear_cgi_arguments(void)
{
   if(!validurl)
     init();
   GCriticalSectionLock lock1(&class_lock);

      // Clear the arrays
   cgi_name_arr.empty();
   cgi_value_arr.empty();

      // And clear everything past the '?' sign in the URL
   const char * ptrurl = url;
   for(const char *ptr = ptrurl; *ptr; ptr++)
     if (*ptr=='?')
       {
         url.setat(ptr-ptrurl, 0);
         break;
       }
}

void
GURL::clear_djvu_cgi_arguments(void)
{
   if(!validurl) init();
      // First - modify the arrays
   GCriticalSectionLock lock(&class_lock);
   for(int i=0;i<cgi_name_arr.size();i++)
   {
      if (cgi_name_arr[i].upcase()==djvuopts)
      {
         cgi_name_arr.resize(i-1);
         cgi_value_arr.resize(i-1);
         break;
      }
   }

      // And store them back into the URL
   store_cgi_args();
}

void
GURL::add_djvu_cgi_argument(const GUTF8String &name, const char * value)
{
   if(!validurl)
     init();
   GCriticalSectionLock lock1(&class_lock);

      // Check if we already have the "DJVUOPTS" argument
   bool have_djvuopts=false;
   for(int i=0;i<cgi_name_arr.size();i++)
   {
      if (cgi_name_arr[i].upcase()==djvuopts)
      {
         have_djvuopts=true;
         break;
      }
   }

      // If there is no DJVUOPTS, insert it
   if (!have_djvuopts)
   {
      int pos=cgi_name_arr.size();
      cgi_name_arr.resize(pos);
      cgi_value_arr.resize(pos);
      cgi_name_arr[pos]=djvuopts;
   }

      // Add new argument to the array
   int pos=cgi_name_arr.size();
   cgi_name_arr.resize(pos);
   cgi_value_arr.resize(pos);
   cgi_name_arr[pos]=name;
   cgi_value_arr[pos]=value;

      // And update the URL
   store_cgi_args();
}

bool
GURL::is_local_file_url(void) const
{
   if(!validurl) const_cast<GURL *>(this)->init();
   GCriticalSectionLock lock((GCriticalSection *) &class_lock);
   return (protocol()=="file" && url[5]==slash);
}

static const int
pathname_start(const GUTF8String &url, const int protolength)
{
  const int length=url.length();
  int retval=0;
  if(protolength+1<length)
  {
    retval=url.search(slash,((url[protolength+1] == '/')
      ?((url[protolength+2] == '/')?(protolength+3):(protolength+2))
      :(protolength+1)));
  }
  return (retval>0)?retval:length;
}

GUTF8String
GURL::pathname(void) const
{
  return (is_local_file_url())
    ?GURL::encode_reserved(UTF8Filename()) 
    :url.substr(pathname_start(url,protocol().length()),(unsigned int)(-1));
}

GURL
GURL::base(void) const
{
   const GUTF8String xurl(get_string());
   const int protocol_length=protocol(xurl).length();
   const char * const url_ptr=xurl;
   const char * ptr, * xslash;
   ptr=xslash=url_ptr+protocol_length+1;
   if(xslash[0] == '/')
   {
     xslash++;
     if(xslash[0] == '/')
       xslash++;
     for(ptr=xslash;ptr[0] && !is_argument(ptr);ptr++)
     {
       if ((ptr[0]==slash)&&ptr[1]&&!is_argument(ptr+1))
        xslash=ptr;
     }
     if(xslash[0] != '/')
     {
       xslash=ptr;
     }
   }
   return GURL::UTF8(GUTF8String(xurl,(int)(xslash-url_ptr))+"/"+ptr);
}

bool
GURL::operator==(const GURL & gurl2) const
{
  const GUTF8String g1(get_string());
  const GUTF8String g2(gurl2.get_string());
  const char *s1 = (const char*)g1;
  const char *s2 = (const char*)g2;
  int n1=0;
  int n2=0;
  while (s1[n1] && !is_argument(s1+n1))
    n1 += 1;
  while (s2[n2] && !is_argument(s2+n2))
    n2 += 1;
  if (n1 == n2)
    return !strcmp(s1+n1,s2+n2) && !strncmp(s1,s2,n1);
  if (n1 == n2+1 && s1[n2]=='/')
    return !strcmp(s1+n1,s2+n2) && !strncmp(s1,s2,n2);
  if (n2 == n1+1 && s2[n1]=='/')
    return !strcmp(s1+n1,s2+n2) && !strncmp(s1,s2,n1);    
  return false;
}

GUTF8String
GURL::name(void) const
{
   if(!validurl)
     const_cast<GURL *>(this)->init();
   GUTF8String retval;
   if(!is_empty())
   {
     const GUTF8String xurl(url);
     const int protocol_length=protocol(xurl).length();
     const char * ptr, * xslash=(const char *)xurl+protocol_length-1;
     for(ptr=(const char *)xurl+protocol_length;
       *ptr && !is_argument(ptr);ptr++)
	 {
       if (*ptr==slash)
          xslash=ptr;
	 }
     retval=GUTF8String(xslash+1, ptr-xslash-1);
   }
   return retval;
}

GUTF8String
GURL::fname(void) const
{
   if(!validurl)
     const_cast<GURL *>(this)->init();
   return decode_reserved(name());
}

GUTF8String
GURL::extension(void) const
{
   if(!validurl)
     const_cast<GURL *>(this)->init();
   GUTF8String xfilename=name();
   GUTF8String retval;

   for(int i=xfilename.length()-1;i>=0;i--)
   {
      if (xfilename[i]=='.')
      {
         retval=(const char*)xfilename+i+1;
         break;
      }
   } 
   return retval;
}

GUTF8String
GURL::decode_reserved(const GUTF8String &gurl)
{
  const char *url=gurl;
  char *res;
  GPBuffer<char> gres(res,gurl.length()+1);
  char *r=res;
  for(const char * ptr=url;*ptr;++ptr,++r)
  {
    if (*ptr!=percent)
    {
      r[0]=*ptr;
    }else
    {
      int c1,c2;
      if ( ((c1=hexval(ptr[1]))>=0)
        && ((c2=hexval(ptr[2]))>=0) )
      {
        r[0]=(c1<<4)|c2;
        ptr+=2;
      } else
      {
        r[0]=*ptr;
      }
    }
  }
  r[0]=0;
  GUTF8String retval(res);
  if(!retval.is_valid())
  {
    retval=GNativeString(res);
  }
  return retval;
}

GUTF8String
GURL::encode_reserved(const GUTF8String &gs)
{
  const char *s=(const char *)gs;
  // Potentially unsafe characters (cf. RFC1738 and RFC1808)
  static const char hex[] = "0123456789ABCDEF";
  
  unsigned char *retval;
  GPBuffer<unsigned char> gd(retval,strlen(s)*3+1);
  unsigned char *d=retval;
  for (; *s; s++,d++)
  {
    // Convert directory separator to slashes
#if defined(_WIN32) || defined(OS2)
    if (*s == backslash || *s== slash)
#else
#ifdef macintosh
    if (*s == colon )
#else
#ifdef UNIX
    if (*s == slash )
#else
#error "Define something here for your operating system"
#endif  
#endif
#endif
    {
      *d = slash; 
      continue;
    }
    unsigned char const ss=(unsigned char const)(*s);
    // WARNING: Whenever you modify this conversion code,
    // make sure, that the following functions are in sync:
    //   encode_reserved()
    //   decode_reserved()
    //   url_to_filename()
    //   filename_to_url()
    // unreserved characters
    if ( (ss>='a' && ss<='z') ||
         (ss>='A' && ss<='Z') ||
         (ss>='0' && ss<='9') ||
         (strchr("$-_.+!*'(),~:=", ss)) ) 
    {
      *d = ss;
      continue;
    }
    // escape sequence
    d[0] = percent;
    d[1] = hex[ (ss >> 4) & 0xf ];
    d[2] = hex[ (ss) & 0xf ];
    d+=2;
  }
  *d = 0;
  return retval;
}

// -------------------------------------------
// Functions for converting filenames and urls
// -------------------------------------------

static GUTF8String
url_from_UTF8filename(const GUTF8String &gfilename)
{
  if(GURL::UTF8(gfilename).is_valid())
  {
    DEBUG_MSG("Debug: URL as Filename: " << gfilename << "\n");
  } 
  const char *filename=gfilename;
  if(filename && (unsigned char)filename[0] == (unsigned char)0xEF
     && (unsigned char)filename[1] == (unsigned char)0xBB 
     && (unsigned char)filename[2] == (unsigned char)0xBF)
  {
    filename+=3;
  }

  // Special case for blank pages
  if(!filename || !filename[0])
  {
    return GUTF8String();
  } 

  // Normalize file name to url slash-and-escape syntax
  GUTF8String oname=GURL::expand_name(filename);
  GUTF8String nname=GURL::encode_reserved(oname);

  // Preprend "file://" to file name. If file is on the local
  // machine, include "localhost".
  GUTF8String url=filespecslashes;
  const char *cnname=nname;
  if (cnname[0] == slash)
  {
    if (cnname[1] == slash)
    {
      url += cnname+2;
    }else
    {
      url = localhost + nname;
    }
  }else
  {
    url += (localhostspec1+2) + nname;
  }
  return url;
}

GUTF8String 
GURL::get_string(const bool nothrow) const
{
  if(!validurl)
    const_cast<GURL *>(this)->init(nothrow);
  return url;
}

// -- Returns a url for accessing a given file.
//    If useragent is not provided, standard url will be created,
//    but will not be understood by some versions if IE.
GUTF8String 
GURL::get_string(const GUTF8String &useragent) const
{
  if(!validurl)
    const_cast<GURL *>(this)->init();
  GUTF8String retval(url);
  if(is_local_file_url()&&useragent.length())
  {
    if(useragent.search("MSIE") >= 0 || useragent.search("Microsoft")>=0)
    {
      retval=filespecslashes + expand_name(UTF8Filename());
    }
  }
  return retval;
}

GURL::UTF8::UTF8(const GUTF8String &xurl)
: GURL(xurl) {}

GURL::UTF8::UTF8(const GUTF8String &xurl,const GURL &codebase)
: GURL(xurl,codebase) {}

GURL::GURL(const GUTF8String &xurl,const GURL &codebase)
  : validurl(false)
{
  if(GURL::UTF8(xurl).is_valid())
    {
      url=xurl;
    }
  else
    {
      // split codebase
      const char *buffer = codebase;
      GUTF8String all(buffer);
      GUTF8String suffix;
      GUTF8String path;
      GUTF8String prefix;
      const int protocol_length=GURL::protocol(all).length();
      const char *start = buffer + pathname_start(all,protocol_length);
      if (start > buffer)
        prefix = GUTF8String(buffer, start-buffer);
      const char *ptr = start;
      while (*ptr && !is_argument(ptr))
        ptr++;
      if (*ptr)
        suffix = GUTF8String(ptr);
      if (ptr > start)
        path = GUTF8String(start, ptr-start);
      // append xurl to path
      const char *c = xurl;
      if(c[0] == slash)
        path = GURL::encode_reserved(xurl);
      else
        path = path + GUTF8String(slash)+GURL::encode_reserved(xurl);
      // construct url
      url = beautify_path(prefix + path + suffix);
    }
}

GURL::Native::Native(const GNativeString &xurl)
: GURL(xurl) {}

GURL::Native::Native(const GNativeString &xurl,const GURL &codebase)
: GURL(xurl,codebase) {}

GURL::GURL(const GNativeString &xurl,const GURL &codebase)
  : validurl(false)
{
  GURL retval(xurl.getNative2UTF8(),codebase);
  if(retval.is_valid())
  {
#if defined(_WIN32)
    // Hack for IE to change \\ to /
    if(retval.is_local_file_url())
    {
      GURL::Filename::UTF8 retval2(retval.UTF8Filename());
      url=retval2.get_string(true);
      validurl=false;
    }else
#endif // WIN32
    {
      url=retval.get_string(true);
      validurl=false;
    }
  }
}

GURL::Filename::Filename(const GNativeString &gfilename)
{
  url=url_from_UTF8filename(gfilename.getNative2UTF8());
}

GURL::Filename::Native::Native(const GNativeString &gfilename)
: GURL::Filename(gfilename) {}

GURL::Filename::Filename(const GUTF8String &gfilename)
{
  url=url_from_UTF8filename(gfilename);
}

GURL::Filename::UTF8::UTF8(const GUTF8String &gfilename)
: GURL::Filename(gfilename) {}

// filename --
// -- Applies heuristic rules to convert a url into a valid file name.  
//    Returns a simple basename in case of failure.
GUTF8String 
GURL::UTF8Filename(void) const
{
  GUTF8String retval;
  if(! is_empty())
  {
    const char *url_ptr=url;
  
    // WARNING: Whenever you modify this conversion code,
    // make sure, that the following functions are in sync:
    //   encode_reserved()
    //   decode_reserved()
    //   url_to_filename()
    //   filename_to_url()

    GUTF8String urlcopy=decode_reserved(url);
    url_ptr = urlcopy;

    // All file urls are expected to start with filespec which is "file:"
    if (GStringRep::cmp(filespec, url_ptr, sizeof(filespec)-1))  //if not
      return GOS::basename(url_ptr);
    url_ptr += sizeof(filespec)-1;
  
#if defined(macintosh)
    //remove all leading slashes
    for(;*url_ptr==slash;url_ptr++)
      EMPTY_LOOP;
    // Remove possible localhost spec
    if ( !GStringRep::cmp(localhost, url_ptr, sizeof(localhost)-1) )
      url_ptr += sizeof(localhost)-1;
    //remove all leading slashes
    while(*url_ptr==slash)
      url_ptr++;
#else
    // Remove possible localhost spec
    if ( !GStringRep::cmp(localhostspec1, url_ptr, sizeof(localhostspec1)-1) )
      // RFC 1738 local host form
      url_ptr += sizeof(localhostspec1)-1;
    else if ( !GStringRep::cmp(localhostspec2, url_ptr, sizeof(localhostspec2)-1 ) )
      // RFC 1738 local host form
      url_ptr += sizeof(localhostspec2)-1;
    else if ( (strlen(url_ptr) > 4)   // "file://<letter>:/<path>"
        && (url_ptr[0] == slash)      // "file://<letter>|/<path>"
        && (url_ptr[1] == slash)
              && isalpha((unsigned char)(url_ptr[2]))
        && ( url_ptr[3] == colon || url_ptr[3] == '|' )
        && (url_ptr[4] == slash) )
      url_ptr += 2;
    else if ( (strlen(url_ptr)) > 2 // "file:/<path>"
        && (url_ptr[0] == slash)
        && (url_ptr[1] != slash) )
      url_ptr++;
#endif

    // Check if we are finished
#if defined(macintosh)
    {
      char *l_url;
      GPBuffer<char> gl_url(l_url,strlen(url_ptr)+1);
      const char *s;
      char *r;
      for ( s=url_ptr,r=l_url; *s; s++,r++)
      {
        *r=(*s == slash)?colon:*s;
      }
      *r=0;
      retval = expand_name(l_url,root);
    }
#else  
    retval = expand_name(url_ptr,root);
#endif
    
#if defined(_WIN32) || defined(OS2)
    if (url_ptr[0] && url_ptr[1]=='|' && url_ptr[2]== slash)
    {
      if ((url_ptr[0]>='a' && url_ptr[0]<='z') 
          || (url_ptr[0]>='A' && url_ptr[0]<='Z'))
      {
	GUTF8String drive;
	drive.format("%c%c%c", url_ptr[0],colon,backslash);
	retval = expand_name(url_ptr+3, drive);
      }
    }
#endif
  }
  // Return what we have
  return retval;
}

GNativeString 
GURL::NativeFilename(void) const
{
  return UTF8Filename().getUTF82Native();
}

#if defined(UNIX) || defined(macintosh) || defined(OS2)
static int
urlstat(const GURL &url,struct stat &buf)
{
  return ::stat(url.NativeFilename(),&buf);
}
#endif

// is_file(url) --
// -- returns true if filename denotes a regular file.
bool
GURL::is_file(void) const
{
  bool retval=false;
  if(is_local_file_url())
  {
#if defined(UNIX) || defined(macintosh) || defined(OS2)
    struct stat buf;
    if (!urlstat(*this,buf))
    {
      retval=!(buf.st_mode & S_IFDIR);
    }
#elif defined(_WIN32)
    GUTF8String filename(UTF8Filename());
    if(filename.length() >= MAX_PATH)
      {
        if(!filename.cmp("\\\\",2))
          filename="\\\\?\\UNC"+filename.substr(1,-1);
        else
          filename="\\\\?\\"+filename;
      }
    wchar_t *wfilename;
    const size_t wfilename_size=filename.length()+1;
    GPBuffer<wchar_t> gwfilename(wfilename,wfilename_size);
    filename.ncopy(wfilename,wfilename_size);
    DWORD dwAttrib;
    dwAttrib = GetFileAttributesW(wfilename);
    if((dwAttrib|1) == 0xFFFFFFFF)
        dwAttrib = GetFileAttributesA(NativeFilename());
    retval=!( dwAttrib & FILE_ATTRIBUTE_DIRECTORY );
#else
# error "Define something here for your operating system"
#endif
  }
  return retval;
}

bool
GURL::is_local_path(void) const
{
  bool retval=false;
  if(is_local_file_url())
  {
#if defined(UNIX) || defined(macintosh) || defined(OS2)
    struct stat buf;
    retval=!urlstat(*this,buf);
#else
    GUTF8String filename(UTF8Filename());
    if(filename.length() >= MAX_PATH)
      {
        if(!filename.cmp("\\\\",2))
          filename="\\\\?\\UNC"+filename.substr(1,-1);
        else
          filename="\\\\?\\"+filename;
      }
    wchar_t *wfilename;
    const size_t wfilename_size=filename.length()+1;
    GPBuffer<wchar_t> gwfilename(wfilename,wfilename_size);
    filename.ncopy(wfilename,wfilename_size);
    DWORD dwAttrib;
    dwAttrib = GetFileAttributesW(wfilename);
    if((dwAttrib|1) == 0xFFFFFFFF)
        dwAttrib = GetFileAttributesA(NativeFilename());
    retval=( (dwAttrib|1) != 0xFFFFFFFF);
#endif
  }
  return retval;
}

// is_dir(url) --
// -- returns true if url denotes a directory.
bool 
GURL::is_dir(void) const
{
  bool retval=false;
  if(is_local_file_url())
  {
    // UNIX implementation
#if defined(UNIX) || defined(macintosh) || defined(OS2)
    struct stat buf;
    if (!urlstat(*this,buf))
    {
      retval=(buf.st_mode & S_IFDIR);
    }
#elif defined(_WIN32)   // (either Windows or WCE)
    GUTF8String filename(UTF8Filename());
    if(filename.length() >= MAX_PATH)
      {
        if(!filename.cmp("\\\\",2))
          filename="\\\\?\\UNC"+filename.substr(1,-1);
        else
          filename="\\\\?\\"+filename;
      }
    wchar_t *wfilename;
    const size_t wfilename_size=filename.length()+1;
    GPBuffer<wchar_t> gwfilename(wfilename,wfilename_size);
    filename.ncopy(wfilename,wfilename_size);
    DWORD dwAttrib;
    dwAttrib = GetFileAttributesW(wfilename);
    if((dwAttrib|1) == 0xFFFFFFFF)
        dwAttrib = GetFileAttributesA(NativeFilename());
    retval=((dwAttrib != 0xFFFFFFFF)&&( dwAttrib & FILE_ATTRIBUTE_DIRECTORY ));
#else
# error "Define something here for your operating system"
#endif
  }
  return retval;
}

// Follows symbolic links.
GURL 
GURL::follow_symlinks(void) const
{
  GURL ret = *this;
#if defined(S_IFLNK)
#if defined(UNIX) || defined(macintosh)
  int lnklen;
  char lnkbuf[MAXPATHLEN+1];
  struct stat buf;
  while ( (urlstat(ret, buf) >= 0) &&
          (buf.st_mode & S_IFLNK) &&
          ((lnklen = readlink(ret.NativeFilename(),lnkbuf,sizeof(lnkbuf))) > 0) )
    {
      lnkbuf[lnklen] = 0;
      GNativeString lnk(lnkbuf);
      ret = GURL(lnk, ret.base());
    }
#endif
#endif
  return ret;
}

int
GURL::mkdir() const
{
  if(! is_local_file_url())
    return -1;
  int retval=0;
  const GURL baseURL=base();
  if (baseURL.get_string() != url && !baseURL.is_dir())
    retval = baseURL.mkdir();
  if(!retval)
    {
#if defined(UNIX)
      if (is_dir())
        retval = 0;
      else 
        retval = ::mkdir(NativeFilename(), 0755);
#elif defined(_WIN32)
      if (is_dir())
        retval = 0;
      else 
        retval = CreateDirectoryA(NativeFilename(), NULL);
#else
# error "Define something here for your operating system"
#endif
    }
  return retval;
}

// deletefile
// -- deletes a file or directory
  
int
GURL::deletefile(void) const
{
  int retval = -1;
  if(is_local_file_url())
    {
#if defined(UNIX)
      if (is_dir())
        retval = ::rmdir(NativeFilename());
      else
        retval = ::unlink(NativeFilename());
#elif defined(_WIN32)
      if (is_dir())
        retval = ::RemoveDirectoryA(NativeFilename());
      else
        retval = ::DeleteFile(NativeFilename());
#else
# error "Define something here for your operating system"
#endif
  }
  return retval;
}

GList<GURL>
GURL::listdir(void) const
{
  GList<GURL> retval;
  if(is_dir())
  {
#if defined(UNIX) || defined(OS2)
    DIR * dir=opendir(NativeFilename());//MBCS cvt
    for(dirent *de=readdir(dir);de;de=readdir(dir))
    {
      const int len = NAMLEN(de);
      if (de->d_name[0]== dot  && len==1)
        continue;
      if (de->d_name[0]== dot  && de->d_name[1]== dot  && len==2)
        continue;
      retval.append(GURL::Native(de->d_name,*this));
    }
    closedir(dir);
#elif defined(_WIN32)
    GURL::UTF8 wildcard("*.*",*this);
    WIN32_FIND_DATA finddata;
    HANDLE handle = FindFirstFile(wildcard.NativeFilename(), &finddata);//MBCS cvt
    const GUTF8String gpathname=pathname();
    const GUTF8String gbase=base().pathname();
    if( handle != INVALID_HANDLE_VALUE)
    {
      do
      {
        GURL::UTF8 Entry(finddata.cFileName,*this);
        const GUTF8String gentry=Entry.pathname();
        if((gentry != gpathname) && (gentry != gbase))
          retval.append(Entry);
      } while( FindNextFile(handle, &finddata) );

      FindClose(handle);
    }
#else
# error "Define something here for your operating system"
#endif
  }
  return retval;
}

int
GURL::cleardir(const int timeout) const
{
  int retval=(-1);
  if(is_dir())
  {
    GList<GURL> dirlist=listdir();
    retval=0;
    for(GPosition pos=dirlist;pos&&!retval;++pos)
    {
      const GURL &Entry=dirlist[pos];
      if(Entry.is_dir())
      {
        if((retval=Entry.cleardir(timeout)) < 0)
        {
          break;
        }
      }
      if(((retval=Entry.deletefile())<0) && (timeout>0))
      {
        GOS::sleep(timeout);
        retval=Entry.deletefile();
      }
    }
  }
  return retval;
}

int
GURL::renameto(const GURL &newurl) const
{
  if (is_local_file_url() && newurl.is_local_file_url())
    return rename(NativeFilename(),newurl.NativeFilename());
  return -1;
}

// expand_name(filename[, fromdirname])
// -- returns the full path name of filename interpreted
//    relative to fromdirname.  Use current working dir when
//    fromdirname is null.
GUTF8String 
GURL::expand_name(const GUTF8String &xfname, const char *from)
{
  const char *fname=xfname;
  GUTF8String retval;
  const size_t maxlen=xfname.length()*9+MAXPATHLEN+10;
  char * const string_buffer = retval.getbuf(maxlen);
  // UNIX implementation
#if defined(UNIX)
  // Perform tilde expansion
  GUTF8String senv;
  if (fname && fname[0]==tilde)
  {
    int n;
    for(n=1;fname[n] && fname[n]!= slash;n++) 
      EMPTY_LOOP;
    struct passwd *pw=0;
    if (n!=1)
    {
      GUTF8String user(fname+1, n-1);
      pw=getpwnam(user);
    }else if ((senv=GOS::getenv("HOME")).length())
    {
      from=(const char *)senv;
      fname = fname + n;
    }else if ((senv=GOS::getenv("LOGNAME")).length())
    {
      pw = getpwnam((const char *)senv.getUTF82Native());
    }else
    {
      pw=getpwuid(getuid());
    }
    if (pw)
    {
      senv=GNativeString(pw->pw_dir).getNative2UTF8();
      from = (const char *)senv;
      fname = fname + n;
    }
    for(;fname[0] == slash; fname++)
      EMPTY_LOOP;
  }
  // Process absolute vs. relative path
  if (fname && fname[0]== slash)
  {
    string_buffer[0]=slash;
    string_buffer[1]=0;
  }else if (from)
  {
    strcpy(string_buffer, expand_name(from));
  }else
  {
    strcpy(string_buffer, GOS::cwd());
  }
  char *s = string_buffer + strlen(string_buffer);
  if(fname)
  {
    for(;fname[0]== slash;fname++)
      EMPTY_LOOP;
    // Process path components
    while(fname[0])
    {
      if (fname[0] == dot )
      {
        if (!fname[1] || fname[1]== slash)
        {
          fname++;
          continue;
        }else if (fname[1]== dot && (fname[2]== slash || !fname[2]))
        {
          fname +=2;
          for(;s>string_buffer+1 && *(s-1)== slash; s--)
            EMPTY_LOOP;
          for(;s>string_buffer+1 && *(s-1)!= slash; s--)
            EMPTY_LOOP;
          continue;
        }
      }
      if ((s==string_buffer)||(*(s-1)!= slash))
      {
        *s = slash;
        s++;
      }
      while (*fname &&(*fname!= slash))
      {
        *s = *fname++;
        if ((size_t)((++s)-string_buffer) > maxlen)
        {
          G_THROW( ERR_MSG("GURL.big_name") );
        }
      }
      *s = 0;
      for(;fname[0]== slash;fname++)
        EMPTY_LOOP;
    }
  }
  if (!fname || !fname[0])
  {
    for(;s>string_buffer+1 && *(s-1) == slash; s--)
      EMPTY_LOOP;
    *s = 0;
  }
#elif defined(_WIN32) // WIN32 implementation
  // Handle base
  strcpy(string_buffer, (char const *)(from ? expand_name(from) : GOS::cwd()));
  //  GNativeString native;
  if (fname)
  {
    char *s = string_buffer;
    char  drv[4];
    // Handle absolute part of fname
    //      Put absolute part of the file name in string_buffer, and
    //      the relative part pointed to by fname.
    if (fname[0]== slash || fname[0]== backslash)
    {
      if (fname[1]== slash || fname[1]== backslash)
      {       // Case "//abcd"
        s[0]=s[1]= backslash; s[2]=0;
      }
      else
      {       // Case "/abcd" or "/"
              //    File is at the root of the current drive. Delete the
              //    slash at the beginning of the filename and leave
              //    an explicit identification of the root of the drive in
              //    string_buffer.
        fname++;
        s[3] = '\0';
      }
    }
    else if (fname[0] && fname[1]==colon)
    {
      if (fname[2]!= slash && fname[2]!= backslash)
      {       // Case "x:abcd"
        if ( toupper((unsigned char)s[0]) != toupper((unsigned char)fname[0])
          || s[1]!=colon)
        {
          drv[0]=fname[0];
          drv[1]=colon;
          drv[2]= dot ;
          drv[3]=0;
          GetFullPathName(drv, maxlen, string_buffer, &s);
          strcpy(string_buffer,(const char *)GUTF8String(string_buffer).getNative2UTF8());
          s = string_buffer;
        }
        fname += 2;
      }
      else if (fname[3]!= slash && fname[3]!= backslash)
      {       // Case "x:/abcd"
        s[0]=toupper((unsigned char)fname[0]);
        s[1]=colon;
        s[2]=backslash;
        s[3]=0;
        fname += 3;
      }
      else
      {       // Case "x://abcd"
        s[0]=s[1]=backslash;
        s[2]=0;
        fname += 4;
      }
    }
    // Process path components
    while(*fname)
    {
      for(;*fname== slash || *fname==backslash;fname++)
        EMPTY_LOOP;
      if (fname[0]== dot )
      {
        if (fname[1]== slash || fname[1]==backslash || !fname[1])
        {
          fname++;
          continue;
        }
		else if ((fname[1] == dot)
                 && (fname[2]== slash || fname[2]==backslash || !fname[2]))
        {
          fname += 2;
          char *back=_tcsrchr(string_buffer,backslash);
          char *forward=_tcsrchr(string_buffer,slash);
          if(back>forward)
          {
            *back=0;
          }else if(forward)
          {
            *forward=0;
          }
          s = string_buffer;
          continue;
        }
      }
      char* s2=s;//MBCS DBCS
      for(;*s;s++) 
        EMPTY_LOOP;
	  if (s > string_buffer && s[-1] != slash && s[-1] != backslash)
        *s++ = backslash;
      while (*fname && (*fname!= slash) && (*fname!=backslash))
      {
        if (s > string_buffer + maxlen)
          G_THROW( ERR_MSG("GURL.big_name") );
        *s++ = *fname++;
      }
      *s = 0;
    }
  }
#else
# error "Define something here for your operating system"
#endif  
  return retval;
}

unsigned int
hash(const GURL & gurl)
{
  unsigned int retval;
  const GUTF8String s(gurl.get_string());
  const int len=s.length();
  if(len && (s[len-1] == '/')) // Don't include the trailing slash as part of the hash.
  {
	retval=hash(s.substr(0,len-1));
  }else
  {
    retval=hash(s);
  }
  return retval;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
