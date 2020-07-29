#ifndef _WDLTYPES_
#define _WDLTYPES_

#ifdef _MSC_VER

typedef __int64 WDL_INT64;
typedef unsigned __int64 WDL_UINT64;

#else

typedef long long WDL_INT64;
typedef unsigned long long WDL_UINT64;

#endif

#ifdef _MSC_VER
  #define WDL_UINT64_CONST(x) (x##ui64)
  #define WDL_INT64_CONST(x) (x##i64)
#else
  #define WDL_UINT64_CONST(x) (x##ULL)
  #define WDL_INT64_CONST(x) (x##LL)
#endif


#if !defined(_MSC_VER) ||  _MSC_VER > 1200
#define WDL_DLGRET INT_PTR CALLBACK
#else
#define WDL_DLGRET BOOL CALLBACK
#endif


#ifdef _WIN32
#include <windows.h>
#else
#include <stdint.h>
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
#endif

#if defined(__ppc__) || !defined(__cplusplus)
typedef char WDL_bool;
#else
typedef bool WDL_bool;
#endif

#ifndef GWLP_USERDATA
#define GWLP_USERDATA GWL_USERDATA
#define GWLP_WNDPROC GWL_WNDPROC
#define GWLP_HINSTANCE GWL_HINSTANCE
#define GWLP_HWNDPARENT GWL_HWNDPARENT
#define DWLP_USER DWL_USER
#define DWLP_DLGPROC DWL_DLGPROC
#define DWLP_MSGRESULT DWL_MSGRESULT
#define SetWindowLongPtr(a,b,c) SetWindowLong(a,b,c)
#define GetWindowLongPtr(a,b) GetWindowLong(a,b)
#define SetWindowLongPtrW(a,b,c) SetWindowLongW(a,b,c)
#define GetWindowLongPtrW(a,b) GetWindowLongW(a,b)
#define SetWindowLongPtrA(a,b,c) SetWindowLongA(a,b,c)
#define GetWindowLongPtrA(a,b) GetWindowLongA(a,b)

#define GCLP_WNDPROC GCL_WNDPROC
#define GCLP_HICON GCL_HICON
#define GCLP_HICONSM GCL_HICONSM
#define SetClassLongPtr(a,b,c) SetClassLong(a,b,c)
#define GetClassLongPtr(a,b) GetClassLong(a,b)
#endif


#ifdef __GNUC__
// for structures that contain doubles, or doubles in structures that are after stuff of questionable alignment (for OSX/linux)
  #define WDL_FIXALIGN  __attribute__ ((aligned (8)))
// usage: void func(int a, const char *fmt, ...) WDL_VARARG_WARN(printf,2,3); // note: if member function, this pointer is counted as well, so as member function that would be 3,4
  #define WDL_VARARG_WARN(x,n,s) __attribute__ ((format (x,n,s)))
  #define WDL_STATICFUNC_UNUSED __attribute__((unused))

#else
  #define WDL_FIXALIGN 
  #define WDL_VARARG_WARN(x,n,s)
  #define WDL_STATICFUNC_UNUSED
#endif

#ifndef WDL_WANT_NEW_EXCEPTIONS
#if defined(__cplusplus)
#include <new>
#define WDL_NEW (std::nothrow)
#endif
#else
#define WDL_NEW
#endif


#if !defined(max) && defined(WDL_DEFINE_MINMAX)
#define max(x,y) ((x)<(y)?(y):(x))
#define min(x,y) ((x)<(y)?(x):(y))
#endif

#ifndef wdl_max
#define wdl_max(x,y) ((x)<(y)?(y):(x))
#define wdl_min(x,y) ((x)<(y)?(x):(y))
#define wdl_abs(x) ((x)<0 ? -(x) : (x))
#define wdl_clamp(x,minv,maxv) ((x) < (minv) ? (minv) : ((x) > (maxv) ? (maxv) : (x)))
#endif

#ifndef _WIN32
  #ifndef strnicmp 
    #define strnicmp(x,y,z) strncasecmp(x,y,z)
  #endif
  #ifndef stricmp 
    #define stricmp(x,y) strcasecmp(x,y)
  #endif
#endif

#ifdef WDL_BACKSLASHES_ARE_ORDINARY
#define WDL_IS_DIRCHAR(x) ((x) == '/')
#else
// for multi-platform applications it seems better to treat backslashes as directory separators even if it
// isn't supported by the underying system (for resolving filenames, etc)
  #ifdef _WIN32
    #define WDL_IS_DIRCHAR(x) ((x) == '\\' || (x) == '/')
  #else
    #define WDL_IS_DIRCHAR(x) ((x) == '/' || (x) == '\\')
  #endif
#endif

#if defined(_WIN32) && !defined(WDL_BACKSLASHES_ARE_ORDINARY)
#define WDL_DIRCHAR '\\'
#define WDL_DIRCHAR_STR "\\"
#else
#define WDL_DIRCHAR '/'
#define WDL_DIRCHAR_STR "/"
#endif

#if defined(_WIN32) || defined(__APPLE__)
  // on __APPLE__ we should ideally check the filesystem for case-sensitivity, assuming a case-insensitive-only match
  #define wdl_filename_cmp(x,y) stricmp(x,y)
  #define wdl_filename_cmpn(x,y,n) strnicmp(x,y,n)
#else
  #define wdl_filename_cmp(x,y) strcmp(x,y)
  #define wdl_filename_cmpn(x,y,n) strncmp(x,y,n)
#endif

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
  #define WDL_likely(x) (__builtin_expect(!!(x),1))
  #define WDL_unlikely(x) (__builtin_expect(!!(x),0))
#else
  #define WDL_likely(x) (!!(x))
  #define WDL_unlikely(x) (!!(x))
#endif

#if defined(_DEBUG) || defined(DEBUG)
#include <assert.h>
#define WDL_ASSERT(x) assert(x)
#define WDL_NORMALLY(x) (assert(x),1)
#define WDL_NOT_NORMALLY(x) (assert(!(x)),0)
#else
#define WDL_ASSERT(x)
#define WDL_NORMALLY(x) WDL_likely(x)
#define WDL_NOT_NORMALLY(x) WDL_unlikely(x)
#endif


typedef unsigned int WDL_TICKTYPE;

static WDL_bool WDL_STATICFUNC_UNUSED WDL_TICKS_IN_RANGE(WDL_TICKTYPE current,  WDL_TICKTYPE refstart, int len) // current >= refstart && current < refstart+len
{
  WDL_ASSERT(len > 0);
  return (current - refstart) < (WDL_TICKTYPE)len;
}

static WDL_bool WDL_STATICFUNC_UNUSED WDL_TICKS_IN_RANGE_ENDING_AT(WDL_TICKTYPE current,  WDL_TICKTYPE refend, int len) // current >= refend-len && current < refend
{
  const WDL_TICKTYPE refstart = refend - len;
  WDL_ASSERT(len > 0);
  return (current - refstart) < (WDL_TICKTYPE)len;
  //return ((refend-1) - current) < (WDL_TICKTYPE)len;
}

// use this if you want validate that nothing that includes wdltypes.h calls fopen() directly on win32
// #define WDL_CHECK_FOR_NON_UTF8_FOPEN

#if defined(WDL_CHECK_FOR_NON_UTF8_FOPEN) && !defined(_WDL_WIN32_UTF8_H_)
  #ifdef fopen
    #undef fopen
  #endif
  #include <stdio.h>
  static WDL_STATICFUNC_UNUSED FILE *WDL_fopenA(const char *fn, const char *mode) { return fopen(fn,mode); }
  #define fopen this_should_be_fopenUTF8_include_win32_utf8.h
#else
  // callers of WDL_fopenA don't mind being non-UTF8-compatible on win32
  // (this could map to either fopen() or fopenUTF8()
  #define WDL_fopenA(fn,mode) fopen(fn,mode)
#endif

#endif
