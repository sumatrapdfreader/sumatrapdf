/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef Vec_h
#define Vec_h

#include "BaseUtil.h"
#include "StrUtil.h"

/* Simple but also optimized for small sizes vector/array class that can
store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

When padding is used, we ensure there's always zeroed <pad> elements at the end.
They're not counted as part of the vector, you can think of them as ensuring
zero-termination generalized to n zero-terminating elements (because n is as
simple to code as 1).

One use case: Vec<char> with padding=1 is C-compatible string buffer.
*/
template <typename T>
class Vec {
protected:
    static const size_t INTERNAL_BUF_SIZE = 16;
    size_t  pad;
    size_t  len;
    size_t  cap;
    T *     els;
    T       buf[INTERNAL_BUF_SIZE];

    void EnsureCap(size_t needed) {
        if (cap >= needed)
            return;

        size_t newCap = cap * 2;
        if (needed > newCap)
            newCap = needed;

        if (newCap + pad >= INT_MAX / sizeof(T)) abort();
        T * newEls = SAZA(T, newCap + pad);
        memcpy(newEls, els, len * sizeof(T));
        FreeEls();
        cap = newCap;
        els = newEls;
    }

    T* MakeSpaceAt(size_t idx, size_t count=1) {
        EnsureCap(len + count);
        T* res = &(els[idx]);
        int tomove = len - idx;
        if (tomove > 0) {
            T* src = els + idx;
            T* dst = els + idx + count;
            memmove(dst, src, tomove * sizeof(T));
        }
        len += count;
        return res;
    }

    void FreeEls() {
        if (els != buf)
            free(els);
    }

public:
    Vec(size_t initCap=0, size_t padding=0) {
        pad = padding;
        els = buf;
        Reset();
        EnsureCap(initCap);
    }

    ~Vec() {
        FreeEls();
    }

    void Reset() {
        len = 0;
        cap = INTERNAL_BUF_SIZE - pad;
        FreeEls();
        els = buf;
        memset(buf, 0, sizeof(buf));
    }

    T& operator[](size_t idx) const {
        return els[idx];
    }

    T& At(size_t idx) const {
        return els[idx];
    }

    size_t Count() const {
        return len;
    }

    void InsertAt(size_t idx, const T& el) {
        MakeSpaceAt(idx, 1)[0] = el;
    }

    void Append(const T& el) {
        InsertAt(len, el);
    }

    void Append(T* src, size_t count) {
        T* dst = MakeSpaceAt(len, count);
        memcpy(dst, src, count * sizeof(T));
    }

    void RemoveAt(size_t idx, size_t count=1) {
        int tomove = len - idx - count;
        if (tomove > 0) {
            T *dst = els + idx;
            T *src = els + idx + count;
            memmove(dst, src, tomove * sizeof(T));
        }
        len -= count;
        memset(els + len, 0, count * sizeof(T));
    }

    void Push(T el) {
        Append(el);
    }

    T& Pop() {
        assert(len > 0);
        if (len > 0)
            len--;
        return At(len);
    }

    T& Last() const {
        assert(len > 0);
        return At(len - 1);
    }

    // perf hack for using as a buffer: client can get accumulated data
    // without duplicate allocation. Note: since Vec over-allocates, this
    // is likely to use more memory than strictly necessary, but in most cases
    // it doesn't matter
    T *StealData() {
        T* res = els;
        if (els == buf)
            res = (T*)memdup(buf, (len + pad) * sizeof(T));
        els = buf;
        Reset();
        return res;
    }

    T *LendData() const {
        return els;
    }

    int Find(T el) const {
        for (size_t i = 0; i < len; i++)
            if (els[i] == el)
                return (int)i;
        return -1;
    }

    void Remove(T el) {
        int i = Find(el);
        if (i > -1)
            RemoveAt(i);
    }
};

// only suitable for T that are pointers that were malloc()ed
template <typename T>
inline void FreeVecMembers(Vec<T>& v)
{
    for (size_t i = 0; i < v.Count(); i++) {
        free(v.At(i));
    }
    v.Reset();
}

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(Vec<T>& v)
{
    for (size_t i = 0; i < v.Count(); i++) {
        delete v.At(i);
    }
    v.Reset();
}


namespace Str {
template <typename T>

class Str : public Vec<T> {
public:
    Str(size_t initCap=0) : Vec(initCap, 1) { }

    void Append(T c)
    {
        MakeSpaceAt(len, 1)[0] = c;
    }

    void Append(const T* src, size_t size=-1)
    {
        if ((size_t)-1 == size)
            size = Len(src);
        T* dst = MakeSpaceAt(len, size);
        memcpy(dst, src, size * sizeof(T));
    }

    void AppendFmt(const T* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        T *res = FmtV(fmt, args);
        AppendAndFree(res);
        va_end(args);
    }

    void AppendAndFree(const T* s)
    {
        if (s)
            Append(s, Len(s));
        free((void*)s);
    }

    void Set(const T* s)
    {
        Reset();
        Append(s);
    }

    T *Get() const
    {
        return els;
    }
};

}

class VStrList : public Vec<TCHAR *>
{
public:
    ~VStrList() {
        FreeVecMembers(*this);
    }

    TCHAR *Join(const TCHAR *joint=NULL) {
        Str::Str<TCHAR> tmp(256);
        size_t jointLen = joint ? Str::Len(joint) : 0;
        for (size_t i = 0; i < Count(); i++) {
            TCHAR *s = At(i);
            if (i > 0 && jointLen > 0)
                tmp.Append(joint, jointLen);
            tmp.Append(s);
        }
        return tmp.StealData();
    }

    int Find(const TCHAR *string) const {
        size_t n = Count();
        for (size_t i = 0; i < n; i++) {
            TCHAR *item = At(i);
            if (Str::Eq(string, item))
                return (int)i;
        }
        return -1;
    }

    /* splits a string into several substrings, separated by the separator
       (optionally collapsing several consecutive separators into one);
       e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
       (resp. "a", "b", "c" if separators are collapsed) */
    size_t Split(const TCHAR *string, const TCHAR *separator, bool collapse=false) {
        size_t start = Count();
        const TCHAR *next;

        while ((next = Str::Find(string, separator))) {
            if (!collapse || next > string)
                Append(Str::DupN(string, next - string));
            string = next + Str::Len(separator);
        }
        if (!collapse || *string)
            Append(Str::Dup(string));

        return Count() - start;
    }

    /* 'cmdLine' contains one or several arguments can be:
        - escaped, in which case it starts with '"', ends with '"' and
          each '"' that is part of the name is escaped with '\\'
        - unescaped, in which case it start with != '"' and ends with ' ' or '\0' */
    size_t ParseCommandLine(const TCHAR *cmdLine)
    {
        size_t start = Count();

        while (cmdLine) {
            // skip whitespace
            while (_istspace(*cmdLine))
                cmdLine++;
            if ('"' == *cmdLine)
                cmdLine = ParseQuoted(cmdLine);
            else if ('\0' != *cmdLine)
                cmdLine = ParseUnquoted(cmdLine);
            else
                cmdLine = NULL;
        }

        return Count() - start;
    }

private:
    /* returns the next character in '*txt' that isn't a backslash */
    static TCHAR SkipBackslashs(const TCHAR *txt)
    {
        assert(txt && '\\' == *txt);
        while ('\\' == *++txt);
        return *txt;
    }

    /* appends the next quoted argument and returns the position after it */
    const TCHAR *ParseQuoted(const TCHAR *arg)
    {
        assert(arg && '"' == *arg);
        arg++;

        Str::Str<TCHAR> txt(Str::Len(arg) / 2);
        const TCHAR *next;
        for (next = arg; *next && *next != '"'; next++) {
            // skip escaped quotation marks according to
            // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
            if ('\\' == *next && '"' == SkipBackslashs(next))
                next++;
            txt.Append(*next);
        }
        this->Append(txt.StealData());

        if ('"' == *next)
            next++;
        return next;
    }

    /* appends the next unquoted argument and returns the position after it */
    const TCHAR *ParseUnquoted(const TCHAR *arg)
    {
        assert(arg && *arg && '"' != *arg && !_istspace(*arg));

        const TCHAR *next;
        // contrary to http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
        // we don't treat quotation marks or backslashes in non-quoted
        // arguments in any special way
        for (next = arg; *next && !_istspace(*next); next++);
        this->Append(Str::DupN(arg, next - arg));

        return next;
    }
};

#endif
