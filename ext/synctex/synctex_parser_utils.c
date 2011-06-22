/* 
Copyright (c) 2008, 2009 jerome DOT laurens AT u-bourgogne DOT fr

This file is part of the SyncTeX package.

Latest Revision: Wed Nov  4 11:52:35 UTC 2009

Version: 1.9
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

#include <limits.h>
#include <ctype.h>
#include <string.h>

#include <sys/stat.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
#define SYNCTEX_WINDOWS 1
#endif

#ifdef _WIN32_WINNT_WINXP
#define SYNCTEX_RECENT_WINDOWS 1
#endif

#ifdef SYNCTEX_WINDOWS
#include <windows.h>
#endif

void *_synctex_malloc(size_t size) {
	void * ptr = malloc(size);
	if(ptr) {
/*  There used to be a switch to use bzero because it is more secure. JL */
		memset(ptr,0, size);
	}
	return (void *)ptr;
}

int _synctex_error(const char * reason,...) {
	va_list arg;
	int result;
	va_start (arg, reason);
#	ifdef SYNCTEX_RECENT_WINDOWS
	{/*	This code is contributed by William Blum.
        As it does not work on some older computers,
        the _WIN32 conditional here is replaced with a SYNCTEX_RECENT_WINDOWS one.
        According to http://msdn.microsoft.com/en-us/library/aa363362(VS.85).aspx
        Minimum supported client	Windows 2000 Professional
        Minimum supported server	Windows 2000 Server
        People running Windows 2K standard edition will not have OutputDebugStringA.
        JL.*/
		char *buff;
		size_t len;
		OutputDebugStringA("SyncTeX ERROR: ");
		len = _vscprintf(reason, arg) + 1;
		buff = (char*)malloc( len * sizeof(char) );
		result = vsprintf(buff, reason, arg) +strlen("SyncTeX ERROR: ");
		OutputDebugStringA(buff);
		OutputDebugStringA("\n");
		free(buff);
	}
#   else
	result = fprintf(stderr,"SyncTeX ERROR: ");
	result += vfprintf(stderr, reason, arg);
	result += fprintf(stderr,"\n");
#   endif
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
#       ifdef	SYNCTEX_WINDOWS
		/*  On Windows, the '\' is also a path separator. */
		while((next = strstr(last_component,"\\"))){
			last_component = next+1;
		}
#       endif
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
synctex_bool_t _synctex_is_equivalent_file_name(const char *lhs, const char *rhs) {
#	if SYNCTEX_WINDOWS
    /*  On Windows, filename should be compared case insensitive.
	 *  The characters '/' and '\' are both valid path separators.
	 *  There will be a very serious problem concerning UTF8 because
	 *  not all the characters must be toupper...
	 *  I would like to have URL's instead of filenames. */
next_character:
    if (!*lhs) {/*  lhs is at the end of the string */
		return *rhs ? synctex_NO : synctex_YES;
	} else if(!*rhs) {/*  rhs is at the end of the string but not lhs */
        return (strcmp(lhs,".tex")==0) ? synctex_YES : synctex_NO;
	}
	else if(SYNCTEX_IS_PATH_SEPARATOR(*lhs)) {/*  lhs points to a path separator */
		if(!SYNCTEX_IS_PATH_SEPARATOR(*rhs)) {/*  but not rhs */
			return synctex_NO;
		}
		/* SumatraPDF: ignore spurious /./ parts (caused by TeXlive 2011) */
		for (; *(lhs + 1) == '.' && SYNCTEX_IS_PATH_SEPARATOR(*(lhs + 2)); lhs += 2);
		for (; *(rhs + 1) == '.' && SYNCTEX_IS_PATH_SEPARATOR(*(rhs + 2)); rhs += 2);
	} else if(SYNCTEX_IS_PATH_SEPARATOR(*rhs)) {/*  rhs points to a path separator but not lhs */
		return synctex_NO;
	}
	else if(SYNCTEX_IS_PATH_SPACE(*lhs)) {/*  lhs points to a path space */
		if(!SYNCTEX_IS_PATH_SPACE(*rhs)) {/*  but not rhs */
			return synctex_NO;
		}
	} else if(SYNCTEX_IS_PATH_SPACE(*rhs)) {/*  rhs points to a path space but not lhs */
		return synctex_NO;
	} else if(toupper(*lhs) != toupper(*rhs)){/*  uppercase do not match */
		return synctex_NO;
	}
	++lhs;
	++rhs;
	goto next_character;
#	else
    return 0 == strcmp(lhs,rhs)?synctex_YES:synctex_NO;
#	endif
}

synctex_bool_t _synctex_path_is_absolute(const char * name) {
	if(!strlen(name)) {
		return synctex_NO;
	}
#	if SYNCTEX_WINDOWS
	if(strlen(name)>2) {
		return (name[1]==':' && SYNCTEX_IS_PATH_SEPARATOR(name[2]))?synctex_YES:synctex_NO;
	}
	return synctex_NO;
#	else
    return SYNCTEX_IS_PATH_SEPARATOR(name[0])?synctex_YES:synctex_NO;
#	endif
}

/*  We do not take care of UTF-8 */
char * _synctex_last_path_component(const char * name) {
	const char * c = name+strlen(name);
	if(c>name) {
		if(!SYNCTEX_IS_PATH_SEPARATOR(*c)) {
			do {
				--c;
				if(SYNCTEX_IS_PATH_SEPARATOR(*c)) {
					return (char *)c+1;
				}
			} while(c>name);
		}
		return (char *)c;/* the last path component is the void string*/
	}
	return (char *)c;
}

int _synctex_copy_with_quoting_last_path_component(const char * src, char ** dest_ref, size_t size) {
  char * lpc;
  if(src && dest_ref) {
#		define dest (*dest_ref)
		dest = NULL;	/*	Default behavior: no change and sucess. */
		lpc = _synctex_last_path_component(src);
		if(strlen(lpc)) {
			if(strchr(lpc,' ') && lpc[0]!='"' && lpc[strlen(lpc)-1]!='"') {
				/*	We are in the situation where adding the quotes is allowed.	*/
				/*	Time to add the quotes.	*/
				/*  Consistency test: we must have dest+size>dest+strlen(dest)+2
				 *	or equivalently: strlen(dest)+2<size (see below) */
				if(strlen(src)<size) {
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
#		undef dest
	}
	return 1; /*  Bad parameter, this value is subject to changes. */
}

/*  The client is responsible of the management of the returned string, if any. */
char * _synctex_merge_strings(const char * first,...);

char * _synctex_merge_strings(const char * first,...) {
	va_list arg;
	size_t size = 0;
	const char * temp;
	/*   First retrieve the size necessary to store the merged string */
	va_start (arg, first);
	temp = first;
	do {
		size_t len = strlen(temp);
		if(UINT_MAX-len<size) {
			_synctex_error("!  _synctex_merge_strings: Capacity exceeded.");
			return NULL;
		}
		size+=len;
	} while( (temp = va_arg(arg, const char *)) != NULL);
	va_end(arg);
	if(size>0) {
		char * result = NULL;
		++size;
		/*  Create the memory storage */
		if(NULL!=(result = (char *)malloc(size))) {
			char * dest = result;
			va_start (arg, first);
			temp = first;
			do {
				if((size = strlen(temp))>0) {
					/*  There is something to merge */
					if(dest != strncpy(dest,temp,size)) {
						_synctex_error("!  _synctex_merge_strings: Copy problem");
						free(result);
						result = NULL;
						return NULL;
					}
					dest += size;
				}
			} while( (temp = va_arg(arg, const char *)) != NULL);
			va_end(arg);
			dest[0]='\0';/*  Terminate the merged string */
			return result;
		}
		_synctex_error("!  _synctex_merge_strings: Memory problem");
		return NULL;
	}
	return NULL;	
}

/*  The purpose of _synctex_get_name is to find the name of the synctex file.
 *  There is a list of possible filenames from which we return the most recent one and try to remove all the others.
 *  With two runs of pdftex or xetex we are sure the the synctex file is really the most appropriate.
 */
int _synctex_get_name(const char * output, const char * build_directory, char ** synctex_name_ref, synctex_compress_mode_t * compress_mode_ref)
{
	if(output && synctex_name_ref && compress_mode_ref) {
#		define synctex_name (*synctex_name_ref)
#		define compress_mode (*compress_mode_ref)
		/*  If output is already absolute, we just have to manage the quotes and the compress mode */
		const char * basename = NULL; /*  base name of output*/
		size_t size = 0;
		/*  Initialize the return values. */
		synctex_name = NULL;
		compress_mode = synctex_compress_mode_none;
		basename = _synctex_last_path_component(output); /*  do not free, output is the owner. */
		/*  Do we have a real base name ? */
		if((size = strlen(basename))>0) {
			/*  Yes, we do. */
			char * temp = NULL;
			char * corename = NULL; /*  base name of output without path extension. */
			char * dirname = NULL; /*  dir name of output */
			char * quoted_corename = NULL;
			char * none = NULL;
			char * gz = NULL;
			char * quoted = NULL;
			char * quoted_gz = NULL;
			char * build = NULL;
			char * build_gz = NULL;
			char * build_quoted = NULL;
			char * build_quoted_gz = NULL;
			struct stat buf;
			time_t time = 0;
			/*  Create corename: let temp point to the dot before the path extension of basename;
			 *  We start form the \0 terminating character and scan the string upward until we find a dot.
			 *  The first dot is not accepted. */
			temp = strrchr(basename,'.');
			size = temp - basename;
			if(size>0) {
				/*  dot properly found, now create corename  */
				if(NULL == (corename = (char *)malloc(size+1))) {
					_synctex_error("!  _synctex_get_name: Memory problem 1");
					return -1;
				}
				if(corename != strncpy(corename,basename,size)) {
					_synctex_error("!  _synctex_get_name: Copy problem 1");
					free(corename);
					dirname = NULL;
					return -2;
				}
				corename[size] = '\0';
			} else {
				/*  There is no path extension,
				 *  Just make a copy of basename */
				corename = _synctex_merge_strings(basename);
			}
			/*  corename is properly set up, owned by "self". */
			/*  creating dirname. */
			size = strlen(output)-strlen(basename);
			if(size>0) {
				/*  output contains more than one path component */
				if(NULL == (dirname = (char *)malloc(size+1))) {
					_synctex_error("!  _synctex_get_name: Memory problem");
					free(corename);
					dirname = NULL;
					return -1;
				}
				if(dirname != strncpy(dirname,output,size)) {
					_synctex_error("!  _synctex_get_name: Copy problem");
					free(dirname);
					dirname = NULL;
					free(corename);
					dirname = NULL;
					return -2;
				}
				dirname[size] = '\0';
			}
			/*  dirname is properly set up. It ends with a path separator, if non void. */
			/*  creating quoted_corename. */
			if(strchr(corename,' ')) {
				quoted_corename = _synctex_merge_strings("\"",corename,"\"");
			}
			/*  quoted_corename is properly set up. */
			if(dirname &&strlen(dirname)>0) {
				none = _synctex_merge_strings(dirname,corename,synctex_suffix,NULL);
				if(quoted_corename && strlen(quoted_corename)>0) {
					quoted = _synctex_merge_strings(dirname,quoted_corename,synctex_suffix,NULL);
				}
			} else {
				none = _synctex_merge_strings(corename,synctex_suffix,NULL);
				if(quoted_corename && strlen(quoted_corename)>0) {
					quoted = _synctex_merge_strings(quoted_corename,synctex_suffix,NULL);
				}
			}
			if(!_synctex_path_is_absolute(output) && build_directory && (size = strlen(build_directory))) {
				temp = (char *)build_directory + size - 1;
				if(_synctex_path_is_absolute(temp)) {
					build = _synctex_merge_strings(build_directory,none,NULL);
					if(quoted_corename && strlen(quoted_corename)>0) {
						build_quoted = _synctex_merge_strings(build_directory,quoted,NULL);
					}
				} else {
					build = _synctex_merge_strings(build_directory,"/",none,NULL);
					if(quoted_corename && strlen(quoted_corename)>0) {
						build_quoted = _synctex_merge_strings(build_directory,"/",quoted,NULL);
					}
				}
			}
			if(none) {
				gz = _synctex_merge_strings(none,synctex_suffix_gz,NULL);
			}
			if(quoted) {
				quoted_gz =	_synctex_merge_strings(quoted,synctex_suffix_gz,NULL);
			}
			if(build) {
				build_gz = _synctex_merge_strings(build,synctex_suffix_gz,NULL);
			}
			if(build_quoted) {
				build_quoted_gz = _synctex_merge_strings(build_quoted,synctex_suffix_gz,NULL);
			}
			/*  All the others names are properly set up... */
			/*  retain the most recently modified file */
#			define TEST(FILENAME,COMPRESS_MODE) \
			if(FILENAME) {\
				if (stat(FILENAME, &buf)) { \
					free(FILENAME);\
					FILENAME = NULL;\
				} else { \
					if(buf.st_mtime>time) { \
						time=buf.st_mtime; \
						synctex_name = FILENAME; \
						compress_mode = COMPRESS_MODE; \
					} \
				} \
			}
			TEST(none,synctex_compress_mode_none);
			TEST(gz,synctex_compress_mode_gz);
			TEST(quoted,synctex_compress_mode_none);
			TEST(quoted_gz,synctex_compress_mode_gz);
			TEST(build,synctex_compress_mode_none);
			TEST(build_gz,synctex_compress_mode_gz);
			TEST(build_quoted,synctex_compress_mode_none);
			TEST(build_quoted_gz,synctex_compress_mode_gz);
#			undef TEST
			/*  Free all the intermediate filenames, except the one that will be used as returned value. */
#			define CLEAN_AND_REMOVE(FILENAME) \
			if(FILENAME && (FILENAME!=synctex_name)) {\
				remove(FILENAME);\
				printf("synctex tool info: %s removed\n",FILENAME);\
				free(FILENAME);\
				FILENAME = NULL;\
			}
			CLEAN_AND_REMOVE(none);
			CLEAN_AND_REMOVE(gz);
			CLEAN_AND_REMOVE(quoted);
			CLEAN_AND_REMOVE(quoted_gz);
			CLEAN_AND_REMOVE(build);
			CLEAN_AND_REMOVE(build_gz);
			CLEAN_AND_REMOVE(build_quoted);
			CLEAN_AND_REMOVE(build_quoted_gz);
#			undef CLEAN_AND_REMOVE
			return 0;
		}
		return -1;/*  bad argument */
#		undef synctex_name
#		undef compress_mode 
	}
	return -2;
}

