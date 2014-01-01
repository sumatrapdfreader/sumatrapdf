/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include <windows.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>

#define SAZA(struct_name, n) (struct_name *)calloc((n), sizeof(struct_name))
#define dimof(X)    (sizeof(X)/sizeof((X)[0]))

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

namespace ucrt {

char *FmtV(const char *fmt, va_list args)
{
    char    message[256];
    size_t  bufCchSize = dimof(message);
    char  * buf = message;
    for (;;)
    {
        int count = _vsnprintf(buf, bufCchSize, fmt, args);
        if ((count >= 0) && ((size_t)count < bufCchSize))
            break;
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != message)
            free(buf);
        if (bufCchSize < 4*1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = SAZA(char, bufCchSize);
        if (!buf)
            break;
    }

    if (buf == message)
        buf = _strdup(message);

    return buf;
}

// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogF(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *s = FmtV(fmt, args);
    OutputDebugStringA(s);
    OutputDebugStringA("\n");
    free(s);
    va_end(args);
}

#if 0
// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogF(const WCHAR *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ScopedMem<WCHAR> s(FmtV(fmt, args));
    // DbgView displays one line per OutputDebugString call
    s.Set(str::Join(s, L"\n"));
    OutputDebugStringW(s.Get());
    va_end(args);
}
#endif

} // namespace ucrt
