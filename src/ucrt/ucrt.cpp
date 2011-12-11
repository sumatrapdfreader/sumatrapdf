/* Copyright 2011-2012 the ucrt project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <typeinfo.h>
#include <math.h>
#include <errno.h>

#pragma comment(linker, "/nodefaultlib:libc.lib")
#pragma comment(linker, "/nodefaultlib:libcmt.lib")

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

void crash_me()
{
    char *p = 0;
    *p = 0;
}

void __cdecl _wassert(const wchar_t *msg, const wchar_t *file, unsigned line)
{
    crash_me();
}

double _wtof(const wchar_t *s)
{
    if (!s) {
        errno = EINVAL;
        return 0;
    }
    size_t len = wcslen(s);
    char *s2 = (char*)malloc(len+1);
    char *tmp = s2;
    while (*s) {
        *tmp++ = (char)*s++;
    }
    double ret = atof(s2);
    free(s2);
    return ret;
}

// _aligned_malloc and _aligned_free are based on:
// http://code.google.com/p/webrtc/source/browse/trunk/src/system_wrappers/source/aligned_malloc.cc
struct AlignedMemory
{
  void* alignedBuffer;
  void* memoryPointer;
};

// http://msdn.microsoft.com/en-us/library/8z34s9c6.aspx
__declspec(noalias) __declspec(restrict)
void* __cdecl _aligned_malloc(size_t size, size_t alignment)
{
    // Don't allow alignment 0 since it's undefined.
    if (alignment == 0)
        return NULL;

    // Make sure that the alignment is an integer power of two or fail.
    if (alignment & (alignment - 1))
        return NULL;

    AlignedMemory* ret = (AlignedMemory*)malloc(sizeof(AlignedMemory));

    // The memory is aligned towards the lowest address that so only
    // alignment - 1 bytes needs to be allocated.
    // A pointer to AlignedMemory must be stored so that it can be retreived for
    // deletion, ergo the sizeof(uintptr_t).
    ret->memoryPointer = malloc(size + sizeof(uintptr_t) +
                                        alignment - 1);
    if (ret->memoryPointer == NULL)
    {
        free(ret);
        return NULL;
    }

    // Alligning after the sizeof(header) bytes will leave room for the header
    // in the same memory block.
    uintptr_t alignStartPos = (uintptr_t)ret->memoryPointer;
    alignStartPos += sizeof(uintptr_t);

    // The buffer should be aligned with 'alignment' bytes. The - 1 guarantees
    // that we align towards the lowest address.
    uintptr_t alignedPos = (alignStartPos + alignment - 1) & ~(alignment - 1);

    // alignedPos is the address sought for.
    ret->alignedBuffer = (void*)alignedPos;

    // Store the address to the AlignedMemory struct in the header so that a
    // it's possible to reclaim all memory.
    uintptr_t headerPos = alignedPos;
    headerPos -= sizeof(uintptr_t);
    void* headerPtr = (void*) headerPos;
    uintptr_t headerValue = (uintptr_t)ret;
    memcpy(headerPtr,&headerValue,sizeof(uintptr_t));

    return ret->alignedBuffer;
}

// http://msdn.microsoft.com/en-us/library/17b5h8td.aspx
__declspec(noalias)
void __cdecl _aligned_free(void* memBlock)
{
    if (memBlock == NULL)
        return;

    uintptr_t alignedPos = (uintptr_t)memBlock;
    uintptr_t headerPos = alignedPos - sizeof(uintptr_t);

    // Read out the address of the AlignedMemory struct from the header.
    uintptr_t* headerPtr = (uintptr_t*)headerPos;
    AlignedMemory* deleteMemory = (AlignedMemory*) *headerPtr;

    free(deleteMemory->memoryPointer);
    delete deleteMemory;
}


}

// provide symbol:
// type_info::'vftable' ["const type_info::`vftable'" (??_7type_info@@6B@)].
// needed when compiling classes with virtual methods with /GR 
type_info::type_info(const type_info& rhs)
{
}

type_info& type_info::operator=(const type_info& rhs)
{
        return *this;
}

type_info::~type_info()
{
}

// debug version of new
void * __cdecl operator new(unsigned int s, int, char const *file, int line) {
  return malloc(s);
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
