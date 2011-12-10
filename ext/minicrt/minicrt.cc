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
 - http://f4b24.googlecode.com/svn/trunk/extra/smartvc9/

More info:
* http://kobyk.wordpress.com/2007/07/20/dynamically-linking-with-msvcrtdll-using-visual-c-2005/
* http://adrianhenke.wordpress.com/2008/12/05/create-lib-file-from-dll/ - info on how to create
  .lib file from .def file
* http://www.ibsensoftware.com/download.html wcrt is another small C runtime library, not
   open source
* http://drdobbs.com/windows/184416623

TODO:
* msvcrt.dll (c:\windows\system32\msvcrt.dll) might contain
  more functionality in later versions of windows. We have
  to make sure we don't use function that are not present in XP
  (by extracting list of symbols from msvcrt.dll on XP with
  dumpbin and not using anything that is not there)
*/


#if 0 // TODO: not sure what the right thing to do is: undefine, leave it alone?
#ifdef UNICODE
#undef UNICODE
#endif

#ifdef _UNICODE
#undef _UNICODE
#endif
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

extern "C" {

/* definitions of functions implemented in msvcrt.dll.
   note: most definition are in standard C library headers */
typedef int (__cdecl *_PIFV)(void);
FILE *  __cdecl __p__iob(void);
int _cdecl _initterm(_PIFV *, _PIFV *);

#pragma section(".CRT$XCA",long,read)
#pragma section(".CRT$XCZ",long,read)
#pragma section(".CRT$XIA",long,read)
#pragma section(".CRT$XIZ",long,read)
#pragma section(".CRT$XPA",long,read)
#pragma section(".CRT$XPZ",long,read)
#pragma section(".CRT$XTA",long,read)
#pragma section(".CRT$XTZ",long,read)

#pragma comment(linker, "/merge:.CRT=.rdata")

__declspec(allocate(".CRT$XIA")) _PIFV __xi_a[] = { 0 };
__declspec(allocate(".CRT$XIZ")) _PIFV __xi_z[] = { 0 }; /* C initializers */
__declspec(allocate(".CRT$XCA")) _PIFV __xc_a[] = { 0 };
__declspec(allocate(".CRT$XCZ")) _PIFV __xc_z[] = { 0 }; /* C++ initializers */
__declspec(allocate(".CRT$XPA")) _PIFV __xp_a[] = { 0 };
__declspec(allocate(".CRT$XPZ")) _PIFV __xp_z[] = { 0 }; /* C pre-terminators */
__declspec(allocate(".CRT$XTA")) _PIFV __xt_a[] = { 0 };
__declspec(allocate(".CRT$XTZ")) _PIFV __xt_z[] = { 0 }; /* C terminators */

void * __cdecl _malloc_dbg(size_t size, int nBlockUse, const char *file,int line)
{
	return malloc(size);
}

void __cdecl _free_dbg(void *data, int nBlockUse)
{
	free(data);
}

void * __cdecl _calloc_dbg(size_t nNum, size_t nSize, int nBlockUse, const char *file, int line)
{
	return calloc(nNum, nSize);
}

char * __cdecl _strdup_dbg(const char *s, int blockType, const char *file, int line)
{
	return _strdup(s);
}

wchar_t * __cdecl _wcsdup_dbg(const wchar_t *s, int blockType, const char *file, int line)
{
	return _wcsdup(s);
}

// TODO: can it be made just an alias to __p_iob ?
FILE * __cdecl __iob_func(void) {
	return __p__iob();
}

}

extern "C" void __cdecl WinMainCRTStartup() {
    int mainret;
    STARTUPINFO StartupInfo = {0};
    GetStartupInfo(&StartupInfo);

    //_atexit_init();

    // call C initializers and C++ constructors
    _initterm(__xi_a, __xi_z);
    _initterm(__xc_a, __xc_z);

    mainret = WinMain(GetModuleHandle(NULL), NULL, NULL,
                      StartupInfo.dwFlags & STARTF_USESHOWWINDOW
                            ? StartupInfo.wShowWindow : SW_SHOWDEFAULT );

    _initterm(__xp_a, __xp_z);
    _initterm(__xt_a, __xt_z);

    ExitProcess(mainret);
}
