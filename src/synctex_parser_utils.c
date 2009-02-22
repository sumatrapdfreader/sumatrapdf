/* 
Copyright (c) 2008 jerome DOT laurens AT u-bourgogne DOT fr

This file is part of the SyncTeX package.

Version: 1.7
See synctex_parser_readme.txt for more details

License:
--------
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE

Except as contained in this notice, the name of the copyright holder  
shall not be used in advertising or otherwise to promote the sale,  
use or other dealings in this Software without prior written  
authorization from the copyright holder.

*/

/*  In this file, we find all the functions that may depend on the operating system. */

#include <synctex_parser_utils.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

void *_synctex_malloc(size_t size) {
	void * ptr = malloc(size);
	if(ptr) {
/*  In Visual C, bzero is not available */
#ifdef _MSC_VER
		memset(ptr,0, size);
#else
		bzero(ptr,size);
#endif
	}
	return (void *)ptr;
}

int _synctex_error(char * reason,...) {
	va_list arg;
	int result;
	va_start (arg, reason);
#	ifdef _WIN32
	{/*	This code is contributed by William Blum */
		char *buff;
		size_t len;
		OutputDebugString("SyncTeX ERROR: ");
		len = _vscprintf(reason, arg) + 1;
		buff = (char*)malloc( len * sizeof(char) );
		result = vsprintf(buff, reason, arg) +strlen("SyncTeX ERROR: ");
		OutputDebugString(buff);
		OutputDebugString("\n");
		free(buff);
	}
#else
	result = fprintf(stderr,"SyncTeX ERROR: ");
	result += vfprintf(stderr, reason, arg);
	result += fprintf(stderr,"\n");
#endif
	va_end (arg);
	return result;
}

/*  strip the last extension of the given string, this string is modified! */
void _synctex_strip_last_path_extension(char * string) {
	if(NULL != string){
		char * last_component = NULL;
		char * last_extension = NULL;
		char * next = NULL;
		/*  first we find the last path component */
		if(NULL == (last_component = strstr(string,"/"))){
			last_component = string;
		} else {
			++last_component;
			while((next = strstr(last_component,"/"))){
				last_component = next+1;
			}
		}
#ifdef	_WIN32
		/*  On Windows, the '\' is also a path separator. */
		while((next = strstr(last_component,"\\"))){
			last_component = next+1;
		}
#endif
		/*  then we find the last path extension */
		if((last_extension = strstr(last_component,"."))){
			++last_extension;
			while((next = strstr(last_extension,"."))){
				last_extension = next+1;
			}
			--last_extension;/*  back to the "." */
			if(last_extension>last_component){/*  filter out paths like ....my/dir/.hidden"*/
				last_extension[0] = '\0';
			}
		}
	}
}

/*  Compare two file names, windows is sometimes case insensitive... */
synctex_bool_t _synctex_is_equivalent_file_name(const char *lhs, const char *rhs, synctex_bool_t append_tex_extension) {
#	if _WIN32
    /*  On Windows, filename should be compared case insensitive.
	 *  The characters '/' and '\' are both valid path separators.
	 *  There will be a very serious problem concerning UTF8 because
	 *  not all the characters must be toupper...
	 *  I would like to have URL's instead of filenames. */
next_character:
  if (!*lhs) {/*  lhs is at the end of the string */
		return *rhs ? synctex_NO : synctex_YES;
	} else if(!*rhs) {/*  rhs is at the end of the string but not lhs */
		return append_tex_extension? _synctex_is_equivalent_file_name(lhs,".tex",synctex_NO): synctex_NO;
	}
	else if(SYNCTEX_IS_PATH_SEPARATOR(*lhs)) {/*  lhs points to a path separator */
		if(!SYNCTEX_IS_PATH_SEPARATOR(*rhs)) {/*  but not rhs */
			return synctex_NO;
		}
	} else if(SYNCTEX_IS_PATH_SEPARATOR(*rhs)) {/*  rhs points to a path separator but not lhs */
		return synctex_NO;
	} else if(toupper(*lhs) != toupper(*rhs)){/*  uppercase do not match */
		return synctex_NO;
	}
	++lhs;
	++rhs;
	goto next_character;
#	elif __EMULATE_W32__
	/*  We should use here some OS specific test because unicode strings are used. */
	size_t len = strlen(lhs);
	if(len==strlen(rhs)) {
		if(strncmp(lhs,rhs,len)) {
			return synctex_NO;
		}
	} else if(len==strlen(rhs)+4){
		len -= 4;
		if(strncmp(lhs,rhs,len)) {
			return synctex_NO;
		}
		return 0 == strncmp(lhs+len,".tex",4) || strncmp(lhs+len,".TEX",4)? synctex_YES:synctex_NO;
	}
    return synctex_YES;
#	else
	/*  We should use here some OS specific test because unicode strings might be used. */
	size_t len = strlen(lhs);
	return (len==strlen(rhs))&&(0 == strncmp(lhs,rhs,len))?synctex_YES:synctex_NO;
#	endif
}

synctex_bool_t _synctex_path_is_absolute(const char * name) {
	if(!strlen(name)) {
		return synctex_NO;
	}
#	if _WIN32
	if(strlen(name)>2) {
		return (name[1]==':' && SYNCTEX_IS_PATH_SEPARATOR(name[2]))?synctex_YES:synctex_NO;
	}
	return synctex_NO;
#	else
    return SYNCTEX_IS_PATH_SEPARATOR(name[0])?synctex_YES:synctex_NO;
#	endif
}

char * _synctex_last_path_component(const char * name) {
	const char * c = name+strlen(name);
	if(c>name) {
		if(!SYNCTEX_IS_PATH_SEPARATOR(*c)) {
			do {
				--c;
				if(SYNCTEX_IS_PATH_SEPARATOR(*c)) {
					break; /*  We found the last path separator */
				}
			} while(c>name);
		}
		return (char *)c+1;/* the last path component is the void string*/
	}
	return (char *)c;
}

int _synctex_copy_with_quoting_last_path_component(const char * src, char ** dest_ref, size_t size) {
  if(src && dest_ref) {
		char * lpc = NULL;
		lpc = _synctex_last_path_component(src);
		if(strlen(lpc)) {
			if(strchr(lpc,' ') && lpc[0]!='"' && lpc[strlen(lpc)-1]!='"') {
				/*	We are in the situation where adding the quotes is allowed.	*/
				/*	Time to add the quotes.	*/
				/*  Consistency test: we must have dest+size>dest+strlen(dest)+2
				 *	or equivalently: strlen(dest)+2<size (see below) */
				if(strlen(src)<size) {
#					define dest (*dest_ref)
					dest = NULL;	/*	Default behavior: no change and sucess. */
					if((dest = (char *)malloc(size+2))) {
						if(dest != strncpy(dest,src,size)) {
							_synctex_error("!  _synctex_copy_with_quoting_last_path_component: Copy problem");
							free(dest);
							dest = NULL;/*  Don't forget to reinitialize. */
							return -2;
						}
						lpc += dest - src;	/*	Now lpc is the last path component of dest.	*/
						memmove(lpc+1,lpc,strlen(lpc)+1);	/*	Also move the null terminating character. */
						lpc[0]='"';
						lpc[strlen(lpc)+1]='\0';/*	Consistency test */
						lpc[strlen(lpc)]='"';
						return 0;	/*	Success. */
					}
					return -1;	/*	Memory allocation error.	*/
				}
				_synctex_error("!  _synctex_copy_with_quoting_last_path_component: Internal inconsistency");
				return -3;
			}
			return 0;	/*	Success. */
		}
		return 0;	/*	No last path component. */
	}
	return 1; /*  Bad parameter, this value is subject to changes. */
}
