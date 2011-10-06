/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef BaseUtil_h
#define BaseUtil_h

#if defined(_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif
#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#endif

#include <windows.h>
#include <unknwn.h>
#include <gdiplus.h>

#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#endif
#include <stdlib.h>
#ifdef DEBUG
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

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

/* Ugly name, but the whole point is to make things shorter.
   SAZA = Struct Allocate and Zero memory for Array
   (note: use operator new for single structs/classes) */
#define SAZA(struct_name, n) (struct_name *)calloc((n), sizeof(struct_name))

#define dimof(X)    (sizeof(X)/sizeof((X)[0]))
#define NoOp()      ((void)0)

/* compile-time assert */
#define CASSERT(exp, name) typedef int assert_##name [(exp) != FALSE]

template <typename T>
inline void swap(T& one, T&two)
{
    T tmp = one; one = two; two = tmp;
}

template <typename T>
inline T limitValue(T val, T min, T max)
{
    assert(max >= min);
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

inline void *memdup(void *data, size_t len)
{
    void *dup = malloc(len);
    if (dup)
        memcpy(dup, data, len);
    return dup;
}
#define _memdup(ptr) memdup(ptr, sizeof(*(ptr)))

// auto-free memory for arbitrary malloc()ed memory of type T*
template <typename T>
class ScopedMem
{
    T *obj;
public:
    ScopedMem() : obj(NULL) {}
    explicit ScopedMem(T* obj) : obj(obj) {}
    ~ScopedMem() { free((void*)obj); }
    void Set(T *o) {
        free((void*)obj);
        obj = o;
    }
    T *Get() const { return obj; }
    T *StealData() {
        T *tmp = obj;
        obj = NULL;
        return tmp;
    }
    operator T*() const { return obj; }
};

class CallbackFunc {
public:
    virtual ~CallbackFunc() { }
    virtual void Callback() = 0;
};

#endif
