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
#include <wchar.h>

#pragma comment(linker, "/nodefaultlib:libc.lib")
#pragma comment(linker, "/nodefaultlib:libcmt.lib")
#pragma comment(linker, "/nodefaultlib:libcmtd.lib")

extern "C" {

/* definitions of functions implemented in msvcrt.dll.
   note: most definition are in standard C library headers */
typedef int (__cdecl *_PIFV)(void);
FILE *  __cdecl __p__iob(void);
int _cdecl _initterm(_PIFV *, _PIFV *);

/* TODO: this is a hack, I clearly don't know what I'm doing here. math.h does:
#define HUGE_VAL _HUGE
There is msvcrt._HUGE symbol but when it was exported in msvcrt.def as "_HUGE",
it was some weird address not even within msvcrt.dll address space.
When I export is as "_HUGE DATA" (following ming-w64), it's no longer visible as
_HUGE to the linker. I'm clearly missing something here, but this is how I work-around this:
*/
typedef union { unsigned char c[8]; double d; } __huge_val_union;
__huge_val_union huge_val = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f }; // only valid for little endian
// note: _HUGE is only available after C static initializers are called, so don't use it before that
double _HUGE = huge_val.d;

int _fltused = 1;

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

void * __cdecl _realloc_dbg(void *mem, size_t newSize, int blockType, const char *file, int line)
{
    return realloc(mem, newSize);
}

void * __cdecl _calloc_dbg(size_t nNum, size_t nSize, int nBlockUse, const char *file, int line)
{
    return calloc(nNum, nSize);
}

void __cdecl _free_dbg(void *data, int nBlockUse)
{
    free(data);
}

char * __cdecl _strdup_dbg(const char *s, int blockType, const char *file, int line)
{
    return _strdup(s);
}

wchar_t * __cdecl _wcsdup_dbg(const wchar_t *s, int blockType, const char *file, int line)
{
    return _wcsdup(s);
}

static void crash_me()
{
    char *p = 0;
    *p = 0;
}

void __cdecl _wassert(const wchar_t *msg, const wchar_t *file, unsigned line)
{
    OutputDebugStringW(msg);
}

// http://msdn.microsoft.com/en-us/library/hc25t012.aspx
double __cdecl _wtof(const wchar_t *s)
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

// http://msdn.microsoft.com/en-us/library/217yyhy9(v=VS.100).aspx
// note: msdn lists the prototype as:
// wchar_t * __cdecl wcspbrk(const wchar_t *str, const wchar_t *strCharSet)
// but only this version works
const wchar_t * __cdecl wcspbrk(const wchar_t *str, const wchar_t *strCharSet)
{
    if (!str || !strCharSet)
        return NULL;

    while (*str) {
        wchar_t c = *str++;
        const wchar_t *tmp = strCharSet;
        while (*tmp) {
            if (c == *tmp++)
                return str;
        }
    }
    return NULL;
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

// Called by the compiler to destruct an array of objects
// This might be governed by 15.2 section of C++ standard (e.g. http://www-d0.fnal.gov/~dladams/cxx_standard.pdf)
// TODO: test me
void __stdcall __ehvec_dtor(void *a, unsigned int objSize, int n, void (__thiscall *dtor)(void *))
{
    char *obj = (char*)a;
    char *end = obj + (n * objSize);
    __try {
        // TODO: should we destruct in reverse as per section 15.2?
        while (obj < end) {
            (*dtor)((void*)obj);
            obj += objSize;
        }
    } __finally {
        // TODO: technically we should exit?
    }
}

/* Called by the compiler to initialize an array of objects e.g. if you have:
static MyClass arrayOfMyClassObjects[10];
the compiler might generate code to call us, where a is a pointer to the beginning
of the memory taken by the array, size is objSize of the object (including padding),
n is size of the array (10 in this case), ctr is a constructor function to be called
for each object and dtr is a destructor
*/
// TODO: test me
void __stdcall __ehvec_ctor(void *a, unsigned int objSize, int n,
    void(__thiscall *ctor)(void*), void(__thiscall *dtor)(void*))
{
    char *obj = (char*)a;
    char *end = obj + (n * objSize);
    __try
    {
        while (obj < end) {
            (*ctor)((void*)obj);
            obj += objSize;
        }
    } __finally {
        if (obj != end) {
            // TODO: could call __ehvec_dtor() instead
            // we haven't constructed all of the objects therefore an exception
            // must have happened, therefore we must destroy partially constructed objects
            char *tmp = (char*)a;
            while (tmp < obj) {
                (*dtor)((void*)tmp);
                tmp += objSize;
            }
        }
    }
}

// TODO: write a proper implementation
__int64 __cdecl _strtoi64(const char *s, char **endptr, int base)
{
    long l = strtol(s, endptr, base);
    return (__int64)l;
}

double __cdecl _difftime64(__time64_t b, __time64_t a)
{
    if ((a < 0) || (b < 0)) {
        errno = EINVAL;
        return 0;
    }

    return (double)(b - a);
}

// http://msdn.microsoft.com/en-us/library/84x924s7(v=VS.100).aspx
size_t __cdecl wcrtomb(char *mbchar, wchar_t wchar, mbstate_t *mbstate)
{
    crash_me();
    return 0;
}

// http://msdn.microsoft.com/en-us/library/tt1kc7c1.aspx
size_t __cdecl mbrlen(const char *str, size_t maxSize, mbstate_t* mbstate)
{
    crash_me();
    return 0;
}

// http://msdn.microsoft.com/en-us/library/5wazc5ys.aspx
size_t __cdecl mbrtowc(wchar_t *wchar, const char *mbchar, size_t cbSize, mbstate_t *mbstate)
{
    crash_me();
    return 0;
}

// http://msdn.microsoft.com/en-us/library/a9yb3dbt.aspx
float __cdecl _hypotf(float x, float y)
{
    float res = (float)_hypot((double)x, (double)y);
    return res;
}

// http://msdn.microsoft.com/en-us/library/14h5k7ff.aspx
int __cdecl _wstat64i32(const wchar_t *path, struct _stat64i32 *buffer)
{
    crash_me();
    return 0;
}

// http://msdn.microsoft.com/en-us/library/14h5k7ff.aspx
int __cdecl _stat64i32(const char *path, struct _stat64i32 *buffer)
{
    crash_me();
    return 0;
}

// http://msdn.microsoft.com/en-us/library/0ys3hc0b.aspx
__int64 __cdecl _ftelli64(FILE *stream)
{
    crash_me();
    return 0;
}

// http://msdn.microsoft.com/en-us/library/75yw9bf3.aspx
int __cdecl _fseeki64(FILE *stream, __int64 offset, int origin)
{
    crash_me();
    return 0;
}

// a bit of gymnastics: Visual Studio headers define vswprintf() as a macro that ultimately expands to
// _vswprintf_c_l(). We don't want to implement _vswprintf_c_l(), we want to re-use vswprintf() in msvcrt.dll
// but we can't say vswprintf() because that'll expand back to _vswprintf_c_l, so we introduced
// msvcrt_vswprintf() alias for msvcrt.vswprintf()
extern int __cdecl msvcrt_vswprintf(wchar_t *buffer, size_t count, const wchar_t *fmt, va_list argptr);
int __cdecl _vswprintf_c_l(wchar_t *dst, size_t count, const wchar_t *fmt, _locale_t locale, va_list argList)
{
    // TODO: in sumatra code (from unrar) this is never called with a locale.
    // Alternatively, we could just ignore locale
    if (locale != 0) {
        crash_me();
    }
    int ret = msvcrt_vswprintf(dst, count, fmt, argList);
    return ret;
}

int __cdecl _CrtSetDbgFlag(int newFlag)
{
    // do nothing, we don't the equivalent of crt's debugging
    return 0;
}

} // extern "C"

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

#if 0
void * __cdecl operator new(unsigned int s)
{
    return malloc(s);
}
#endif

#if 0
void __cdecl operator delete(void* p)
{
    free(p);
}
#endif

#if 0
void * __cdecl operator new[](size_t n)
{
    return operator new(n);
}
#endif

void __cdecl operator delete[](void* p)
{
    operator delete(p);
}

// debug version of new
void * __cdecl operator new(unsigned int s, int, char const *file, int line)
{
    return malloc(s);
}

// debug version of new[]
void * __cdecl operator new[](unsigned int s, int, char const *file, int line)
{
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
