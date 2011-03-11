/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef BaseUtil_h
#define BaseUtil_h

#if defined(_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif
#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#endif

#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#endif
#include <stdlib.h>
#ifdef DEBUG
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include <windows.h>
#include <tchar.h>

/* Few most common includes for C stdlib */
#include <assert.h>
#include <stdio.h>

#ifndef _UNICODE
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <wchar.h>
#include <string.h>

/* Ugly names but the whole point is to make things shorter.
   SA = Struct Allocate
   SAZ = Struct Allocate and Zero memory
   SAZA = Struct Allocate and Zero memory for Array */
#define SA(struct_name) (struct_name *)malloc(sizeof(struct_name))
#define SAZA(struct_name, n) (struct_name *)calloc((n), sizeof(struct_name))
#define SAZ(struct_name) SAZA(struct_name, 1)

#define dimof(X)    (sizeof(X)/sizeof((X)[0]))
#define NoOp()      ((void)0)

/* TODO: consider using standard C macros for SWAP */
static inline void swap_int(int& one, int& two)
{
    int tmp = one; one = two; two = one;
}
static inline void swap_double(double& one, double& two)
{
    double tmp = one; one = two; two = one;
}

static inline void *memdup(void *data, size_t len)
{
    void *dup = malloc(len);
    if (dup)
        memcpy(dup, data, len);
    return dup;
}
#define _memdup(ptr) memdup(ptr, sizeof(*(ptr)))

class ScopedCritSec
{
    CRITICAL_SECTION *cs;
public:
    explicit ScopedCritSec(CRITICAL_SECTION *cs) {
        this->cs = cs;
        EnterCriticalSection(this->cs);
    }
    ~ScopedCritSec() {
        LeaveCriticalSection(this->cs);
    }
};

// auto-free memory for arbitrary malloc()ed memory of type T*
template <typename T>
class ScopedMem
{
    T *obj;
public:
    explicit ScopedMem(T* obj) : obj(obj) {}
    ~ScopedMem() { free((void*)obj); }
    T *Get() const { return obj; }
    operator T*() const { return obj; }
};

class CallbackFunc
{
public:
    virtual void Callback(void *arg=NULL) = 0;
};

#endif
