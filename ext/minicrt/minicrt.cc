/*
This is a single-file implementation of Visual Studio crt
made in order to reduce the size of executables.

This is preliminary, I might change my mind or never
finish this code, but this is the plan:
* implementation is in a single file, to make it easy to integrate
  in Sumatra and other projects
* I'll start from scratch i.e. start with no code at all and
  add the necessary functions one by one, guided by what
  linker tells us is missing. This is to learn what is the absolutely
  minimum to include and review all the code that is included
* I'll reuse as much as possible from msvcrt.dll
* I'll use the code already written in omaha's minicrt project
  (but only after reviewing each function)
* the code that comes in *.obj files will have to be written
* other places that I might steal the code from:
 - http://llvm.org/svn/llvm-project/compiler-rt/trunk/
 - http://www.jbox.dk/sanos/source/

More info:
* http://kobyk.wordpress.com/2007/07/20/dynamically-linking-with-msvcrtdll-using-visual-c-2005/
* http://adrianhenke.wordpress.com/2008/12/05/create-lib-file-from-dll/ - info on how to create
  .lib file from .def file

TODO:
* msvcrt.dll (c:\windows\system32\msvcrt.dll) might contain
  more functionality in later versions of windows. We have
  to make sure we don't use function that are not present in XP
  (by extracting list of symbols from msvcrt.dll on XP with
  dumpbin and not using anything that is not there)
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef UNICODE
#undef UNICODE
#endif

#ifdef _UNICODE
#undef _UNICODE
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

/* definitions of functions implemented in msvcrt.dll */
extern "C" {
//void *  __cdecl malloc(size_t size);
//void    __cdecl free(void * p);
//void *  __cdecl calloc(size_t nitems, size_t size);
extern FILE *  __cdecl __p__iob(void);
}

extern "C" {

// TODO: is there a way (in C) to make them just aliases instead of
// calling them?
void * __cdecl _malloc_dbg(size_t size) {
	return malloc(size); // from msvcrt
}

void __cdecl _free_dbg(void * p) {
	return free(p);
}

void * __cdecl _calloc_dbg(size_t nitems, size_t size) {
	return calloc(nitems, size);
}

FILE * __cdecl __iob_func(void) {
	return __p__iob();
}

}