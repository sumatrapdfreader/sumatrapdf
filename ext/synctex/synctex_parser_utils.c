/* 
 Copyright (c) 2008-2017 jerome DOT laurens AT u-bourgogne DOT fr
 
 This file is part of the __SyncTeX__ package.
 
 [//]: # (Latest Revision: Fri Jul 14 16:20:41 UTC 2017)
 [//]: # (Version: 1.21)
 
 See `synctex_parser_readme.md` for more details
 
 ## License
 
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

#include <sys/stat.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
#define SYNCTEX_WINDOWS 1
#endif

#if defined(__OS2__)
#define SYNCTEX_OS2 1
#endif

#if defined(_WIN32)
#define SYNCTEX_RECENT_WINDOWS 1
#endif

#ifdef SYNCTEX_WINDOWS
#include <windows.h>
#include <shlwapi.h> /* Use shlwapi.lib */
#endif

void *_synctex_malloc(size_t size) {
    void * ptr = malloc(size);
    if(ptr) {
        memset(ptr,0, size);/* ensures null termination of strings */
    }
    return (void *)ptr;
}

void _synctex_free(void * ptr) {
    if (ptr) {
        free(ptr);
    }
}

#if !defined(_WIN32)
#   include <syslog.h>
#endif

SYNCTEX_ATTRIBUTE_FORMAT_PRINTF(3, 0)
static int _synctex_log(int level, const char * prompt, const char * reason,va_list arg) {
	int result;
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
		OutputDebugStringA(prompt);
		len = _vscprintf(reason, arg) + 1;
		buff = (char*)malloc( len * sizeof(char) );
		result = vsprintf(buff, reason, arg) +strlen(prompt);
		OutputDebugStringA(buff);
		OutputDebugStringA("\n");
		free(buff);
	}
#   elif SYNCTEX_USE_SYSLOG
    char * buffer1 = NULL;
    char * buffer2 = NULL;
    openlog ("SyncTeX", LOG_CONS | LOG_PID | LOG_PERROR | LOG_NDELAY, LOG_LOCAL0);
    if (vasprintf(&buffer1,reason,arg)>=0
        && asprintf(&buffer2,"%s%s",prompt, buffer1)>=0) {
        syslog (level, "%s", buffer2);
        result = (int)strlen(buffer2);
    } else {
        syslog (level, "%s",prompt);
        vsyslog(level,reason,arg);
        result = (int)strlen(prompt);
    }
    free(buffer1);
    free(buffer2);
    closelog();
#   else
    FILE * where = level == LOG_ERR? stderr: stdout;
    result = fputs(prompt,where);
    result += vfprintf(where, reason, arg);
    result += fprintf(where,"\n");
#   endif
	return result;
}

/* Not needed here, defined in SumatraPDF */
#if 0
SYNCTEX_ATTRIBUTE_FORMAT_PRINTF(1, 2)
int _synctex_error(const char * reason,...) {
    va_list arg;
    int result;
    va_start (arg, reason);
#if defined(SYNCTEX_RECENT_WINDOWS) /* LOG_ERR is not used */
    result = _synctex_log(0, "! SyncTeX Error : ", reason, arg);
#else
    result = _synctex_log(LOG_ERR, "! SyncTeX Error : ", reason, arg);
#endif
    va_end (arg);
    return result;
}

SYNCTEX_ATTRIBUTE_FORMAT_PRINTF(1, 2)
int _synctex_debug(const char * reason,...) {
    va_list arg;
    int result;
    va_start (arg, reason);
#if defined(SYNCTEX_RECENT_WINDOWS) /* LOG_DEBUG is not used */
    result = _synctex_log(0, "! SyncTeX Error : ", reason, arg);
#else
    result = _synctex_log(LOG_DEBUG, "! SyncTeX Error : ", reason, arg);
#endif
    va_end (arg);
    return result;
}
#endif

/*  strip the last extension of the given string, this string is modified! */
void _synctex_strip_last_path_extension(char * string) {
	if(NULL != string){
		char * last_component = NULL;
		char * last_extension = NULL;
#       if defined(SYNCTEX_WINDOWS)
    /* UNICODE defined in SumatraPDF, explicitly use ...A version */
		last_component = PathFindFileNameA(string);
		last_extension = PathFindExtensionA(string);
		if(last_extension == NULL)return;
		if(last_component == NULL)last_component = string;
		if(last_extension>last_component){/* filter out paths like "my/dir/.hidden" */
			last_extension[0] = '\0';
		}
#       else
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
#               if defined(SYNCTEX_OS2)
		/*  On OS2, the '\' is also a path separator. */
		while((next = strstr(last_component,"\\"))){
			last_component = next+1;
		}
#               endif /* SYNCTEX_OS2 */
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
#       endif /* SYNCTEX_WINDOWS */
	}
}

synctex_bool_t synctex_ignore_leading_dot_slash_in_path(const char ** name_ref)
{
    if (SYNCTEX_IS_DOT((*name_ref)[0]) && SYNCTEX_IS_PATH_SEPARATOR((*name_ref)[1])) {
        do {
            (*name_ref) += 2;
            while (SYNCTEX_IS_PATH_SEPARATOR((*name_ref)[0])) {
                ++(*name_ref);
            }
        } while(SYNCTEX_IS_DOT((*name_ref)[0]) && SYNCTEX_IS_PATH_SEPARATOR((*name_ref)[1]));
        return synctex_YES;
    }
    return synctex_NO;
}

/*  The base name is necessary to deal with the 2011 file naming convention...
 *  path is a '\0' terminated string
 *  The return value is the trailing part of the argument,
 *  just following the first occurrence of the regexp pattern "[^|/|\].[\|/]+".*/
const char * _synctex_base_name(const char *path) {
    const char * ptr = path;
    do {
        if (synctex_ignore_leading_dot_slash_in_path(&ptr)) {
            return ptr;
        }
        do {
            if (!*(++ptr)) {
                return path;
            }
        } while (!SYNCTEX_IS_PATH_SEPARATOR(*ptr));
    } while (*(++ptr));
    return path;
}

/*  Compare two file names, windows is sometimes case insensitive... */
synctex_bool_t _synctex_is_equivalent_file_name(const char *lhs, const char *rhs) {
    /*  Remove the leading regex '(\./+)*' in both rhs and lhs */
    synctex_ignore_leading_dot_slash_in_path(&lhs);
    synctex_ignore_leading_dot_slash_in_path(&rhs);
next_character:
	if (SYNCTEX_IS_PATH_SEPARATOR(*lhs)) {/*  lhs points to a path separator */
		if (!SYNCTEX_IS_PATH_SEPARATOR(*rhs)) {/*  but not rhs */
			return synctex_NO;
		}
        ++lhs;
        ++rhs;
        synctex_ignore_leading_dot_slash_in_path(&lhs);
        synctex_ignore_leading_dot_slash_in_path(&rhs);
        goto next_character;
	} else if (SYNCTEX_IS_PATH_SEPARATOR(*rhs)) {/*  rhs points to a path separator but not lhs */
		return synctex_NO;
	} else if (SYNCTEX_ARE_PATH_CHARACTERS_EQUAL(*lhs,*rhs)){/*  uppercase do not match */
		return synctex_NO;
	} else if (!*lhs) {/*  lhs is at the end of the string */
		return *rhs ? synctex_NO : synctex_YES;
	} else if(!*rhs) {/*  rhs is at the end of the string but not lhs */
		return synctex_NO;
	}
	++lhs;
	++rhs;
	goto next_character;
}

synctex_bool_t _synctex_path_is_absolute(const char * name) {
	if(!strlen(name)) {
		return synctex_NO;
	}
#	if defined(SYNCTEX_WINDOWS) || defined(SYNCTEX_OS2)
	if(strlen(name)>2) {
		return (name[1]==':' && SYNCTEX_IS_PATH_SEPARATOR(name[2]))?synctex_YES:synctex_NO;
	}
	return synctex_NO;
#	else
    return SYNCTEX_IS_PATH_SEPARATOR(name[0])?synctex_YES:synctex_NO;
#	endif
}

/*  We do not take care of UTF-8 */
const char * _synctex_last_path_component(const char * name) {
	const char * c = name+strlen(name);
	if(c>name) {
		if(!SYNCTEX_IS_PATH_SEPARATOR(*c)) {
			do {
				--c;
				if(SYNCTEX_IS_PATH_SEPARATOR(*c)) {
					return c+1;
				}
			} while(c>name);
		}
		return c;/* the last path component is the void string*/
	}
	return c;
}

int _synctex_copy_with_quoting_last_path_component(const char * src, char ** dest_ref, size_t size) {
  if(src && dest_ref) {
      const char * lpc;
#		define dest (*dest_ref)
		dest = NULL;	/*	Default behavior: no change and success. */
		lpc = _synctex_last_path_component(src);
		if(strlen(lpc)) {
			if(strchr(lpc,' ') && lpc[0]!='"' && lpc[strlen(lpc)-1]!='"') {
				/*	We are in the situation where adding the quotes is allowed.	*/
				/*	Time to add the quotes.	*/
				/*  Consistency test: we must have dest+size>dest+strlen(dest)+2
				 *	or equivalently: strlen(dest)+2<size (see below) */
				if(strlen(src)<size) {
					if((dest = (char *)malloc(size+2))) {
						char * dpc = dest + (lpc-src);	/*	dpc is the last path component of dest.	*/
						if(dest != strncpy(dest,src,size)) {
							_synctex_error("!  _synctex_copy_with_quoting_last_path_component: Copy problem");
							free(dest);
							dest = NULL;/*  Don't forget to reinitialize. */
							return -2;
						}
						memmove(dpc+1,dpc,strlen(dpc)+1);	/*	Also move the null terminating character. */
						dpc[0]='"';
						dpc[strlen(dpc)+1]='\0';/*	Consistency test */
						dpc[strlen(dpc)]='"';
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
			va_end(arg);
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
						va_end(arg);
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
 *  With two runs of pdftex or xetex we are sure the synctex file is really the most appropriate.
 */
int _synctex_get_name(const char * output, const char * build_directory, char ** synctex_name_ref, synctex_io_mode_t * io_mode_ref)
{
	if(output && synctex_name_ref && io_mode_ref) {
		/*  If output is already absolute, we just have to manage the quotes and the compress mode */
		size_t size = 0;
        char * synctex_name = NULL;
        synctex_io_mode_t io_mode = *io_mode_ref;
		const char * base_name = _synctex_last_path_component(output); /*  do not free, output is the owner. base name of output*/
		/*  Do we have a real base name ? */
		if(strlen(base_name)>0) {
			/*  Yes, we do. */
			const char * temp = NULL;
			char * core_name = NULL; /*  base name of output without path extension. */
			char * dir_name = NULL; /*  dir name of output */
			char * quoted_core_name = NULL;
			char * basic_name = NULL;
			char * gz_name = NULL;
			char * quoted_name = NULL;
			char * quoted_gz_name = NULL;
			char * build_name = NULL;
			char * build_gz_name = NULL;
			char * build_quoted_name = NULL;
			char * build_quoted_gz_name = NULL;
			struct stat buf;
			time_t the_time = 0;
			/*  Create core_name: let temp point to the dot before the path extension of base_name;
			 *  We start form the \0 terminating character and scan the string upward until we find a dot.
			 *  The leading dot is not accepted. */
			if((temp = strrchr(base_name,'.')) && (size = temp - base_name)>0) {
				/*  There is a dot and it is not at the leading position    */
				if(NULL == (core_name = (char *)malloc(size+1))) {
					_synctex_error("!  _synctex_get_name: Memory problem 1");
					return -1;
				}
				if(core_name != strncpy(core_name,base_name,size)) {
					_synctex_error("!  _synctex_get_name: Copy problem 1");
					free(core_name);
					dir_name = NULL;
					return -2;
				}
				core_name[size] = '\0';
			} else {
				/*  There is no path extension,
				 *  Just make a copy of base_name */
				core_name = _synctex_merge_strings(base_name);
			}
			/*  core_name is properly set up, owned by "self". */
			/*  creating dir_name. */
			size = strlen(output)-strlen(base_name);
			if(size>0) {
				/*  output contains more than one path component */
				if(NULL == (dir_name = (char *)malloc(size+1))) {
					_synctex_error("!  _synctex_get_name: Memory problem");
					free(core_name);
					return -1;
				}
				if(dir_name != strncpy(dir_name,output,size)) {
					_synctex_error("!  _synctex_get_name: Copy problem");
					free(dir_name);
					dir_name = NULL;
					free(core_name);
					dir_name = NULL;
					return -2;
				}
				dir_name[size] = '\0';
			}
			/*  dir_name is properly set up. It ends with a path separator, if non void. */
			/*  creating quoted_core_name. */
			if(strchr(core_name,' ')) {
				quoted_core_name = _synctex_merge_strings("\"",core_name,"\"");
			}
			/*  quoted_core_name is properly set up. */
			if(dir_name &&strlen(dir_name)>0) {
				basic_name = _synctex_merge_strings(dir_name,core_name,synctex_suffix,NULL);
				if(quoted_core_name && strlen(quoted_core_name)>0) {
					quoted_name = _synctex_merge_strings(dir_name,quoted_core_name,synctex_suffix,NULL);
				}
			} else {
				basic_name = _synctex_merge_strings(core_name,synctex_suffix,NULL);
				if(quoted_core_name && strlen(quoted_core_name)>0) {
					quoted_name = _synctex_merge_strings(quoted_core_name,synctex_suffix,NULL);
				}
			}
			if(!_synctex_path_is_absolute(output) && build_directory && (size = strlen(build_directory))) {
				temp = build_directory + size - 1;
				if(_synctex_path_is_absolute(temp)) {
					build_name = _synctex_merge_strings(build_directory,basic_name,NULL);
					if(quoted_core_name && strlen(quoted_core_name)>0) {
						build_quoted_name = _synctex_merge_strings(build_directory,quoted_name,NULL);
					}
				} else {
					build_name = _synctex_merge_strings(build_directory,"/",basic_name,NULL);
					if(quoted_core_name && strlen(quoted_core_name)>0) {
						build_quoted_name = _synctex_merge_strings(build_directory,"/",quoted_name,NULL);
					}
				}
			}
			if(basic_name) {
				gz_name = _synctex_merge_strings(basic_name,synctex_suffix_gz,NULL);
			}
			if(quoted_name) {
				quoted_gz_name = _synctex_merge_strings(quoted_name,synctex_suffix_gz,NULL);
			}
			if(build_name) {
				build_gz_name = _synctex_merge_strings(build_name,synctex_suffix_gz,NULL);
			}
			if(build_quoted_name) {
				build_quoted_gz_name = _synctex_merge_strings(build_quoted_name,synctex_suffix_gz,NULL);
			}
			/*  All the others names are properly set up... */
			/*  retain the most recently modified file */
#			define TEST(FILENAME,COMPRESS_MODE) \
			if(FILENAME) {\
				if (stat(FILENAME, &buf)) { \
					free(FILENAME);\
					FILENAME = NULL;\
				} else if (buf.st_mtime>the_time) { \
                    the_time=buf.st_mtime; \
                    synctex_name = FILENAME; \
                    if (COMPRESS_MODE) { \
                        io_mode |= synctex_io_gz_mask; \
                    } else { \
                        io_mode &= ~synctex_io_gz_mask; \
                    } \
				} \
			}
			TEST(basic_name,synctex_DONT_COMPRESS);
			TEST(gz_name,synctex_COMPRESS);
			TEST(quoted_name,synctex_DONT_COMPRESS);
			TEST(quoted_gz_name,synctex_COMPRESS);
			TEST(build_name,synctex_DONT_COMPRESS);
			TEST(build_gz_name,synctex_COMPRESS);
			TEST(build_quoted_name,synctex_DONT_COMPRESS);
			TEST(build_quoted_gz_name,synctex_COMPRESS);
#			undef TEST
			/*  Free all the intermediate filenames, except the one that will be used as returned value. */
#			define CLEAN_AND_REMOVE(FILENAME) \
			if(FILENAME && (FILENAME!=synctex_name)) {\
				remove(FILENAME);\
				printf("synctex tool info: %s removed\n",FILENAME);\
				free(FILENAME);\
				FILENAME = NULL;\
			}
			CLEAN_AND_REMOVE(basic_name);
			CLEAN_AND_REMOVE(gz_name);
			CLEAN_AND_REMOVE(quoted_name);
			CLEAN_AND_REMOVE(quoted_gz_name);
			CLEAN_AND_REMOVE(build_name);
			CLEAN_AND_REMOVE(build_gz_name);
			CLEAN_AND_REMOVE(build_quoted_name);
			CLEAN_AND_REMOVE(build_quoted_gz_name);
#			undef CLEAN_AND_REMOVE
            /* set up the returned values */
            * synctex_name_ref = synctex_name;
            /* synctex_name won't always end in .gz, even when compressed. */
            FILE * F = fopen(synctex_name, "r");
            if (F != NULL) {
                if (!feof(F)
                && 31 == fgetc(F)
                && !feof(F)
                && 139 == fgetc(F)) {
                    io_mode = synctex_compress_mode_gz;
                }
                fclose(F);
            }
            * io_mode_ref = io_mode;
			return 0;
		}
		return -1;/*  bad argument */
	}
	return -2;
}

const char * _synctex_get_io_mode_name(synctex_io_mode_t io_mode) {
    static const char * synctex_io_modes[4] = {"r","rb","a","ab"}; 
    unsigned index = ((io_mode & synctex_io_gz_mask)?1:0) + ((io_mode & synctex_io_append_mask)?2:0);// bug pointed out by Jose Alliste
    return synctex_io_modes[index];
}
