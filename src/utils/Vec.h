/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

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

    T* MakeSpaceAt(size_t idx, size_t count) {
        EnsureCap(len + count);
        T* res = &(els[idx]);
        if (len > idx) {
            T* src = els + idx;
            T* dst = els + idx + count;
            memmove(dst, src, (len - idx) * sizeof(T));
        }
        LenIncrease(count);
        return res;
    }

    void FreeEls() {
        if (els != buf)
            free(els);
    }

public:
    Vec(size_t initCap=0, size_t padding=0) : pad(padding) {
        els = buf;
        Reset();
        EnsureCap(initCap);
    }
    ~Vec() {
        FreeEls();
    }

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    Vec(const Vec& orig) : pad(orig.pad) {
        els = buf;
        Reset();
        EnsureCap(orig.cap);
        // use memcpy, as Vec only supports POD types
        memcpy(els, orig.els, sizeof(T) * (len = orig.len));
    }
    Vec& operator=(const Vec& that) {
        if (this != &that) {
            Reset();
            EnsureCap(that.cap);
            // use memcpy, as Vec only supports POD types
            memcpy(els, that.els, sizeof(T) * (len = that.len));
        }
        return *this;
    }

    void Reset() {
        len = 0;
        cap = INTERNAL_BUF_SIZE - pad;
        FreeEls();
        els = buf;
        memset(buf, 0, sizeof(buf));
    }

    // ensures empty space at the end of the list where items can
    // be appended through ReadFile or memcpy (don't forget to call
    // LenIncrease once you know how many items have been added)
    // and returns a pointer to the first empty spot
    // Note: use AppendBlanks if you know the number of items in advance
    T *EnsureEndPadding(size_t count) {
        EnsureCap(len + count);
        return &els[len];
    }

    void LenIncrease(size_t count) {
        len += count;
    }

    T& At(size_t idx) const {
        return els[idx];
    }
    // use &At() if you need a pointer to the element (e.g. if T is a struct)

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
        T* dst = AppendBlanks(count);
        memcpy(dst, src, count * sizeof(T));
    }

    T* AppendBlanks(size_t count) {
        return MakeSpaceAt(len, count);
    }

    void RemoveAt(size_t idx, size_t count=1) {
        if (len > idx + count) {
            T *dst = els + idx;
            T *src = els + idx + count;
            memmove(dst, src, (len - idx - count) * sizeof(T));
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

    int Find(T el, size_t startAt=0) const {
        for (size_t i = startAt; i < len; i++)
            if (els[i] == el)
                return (int)i;
        return -1;
    }

    void Remove(T el) {
        int i = Find(el);
        if (i > -1)
            RemoveAt(i);
    }

    void Sort(int (*cmpFunc)(const void *a, const void *b)) {
        qsort(els, len, sizeof(T), cmpFunc);
    }

    void Reverse() {
        for (size_t i = 0; i < len / 2; i++)
            swap(els[i], els[len - i - 1]);
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


namespace str {
template <typename T>

class Str : public Vec<T> {
public:
    Str(size_t initCap=0) : Vec(initCap, 1) { }

    void Append(T c)
    {
        AppendBlanks(1)[0] = c;
    }

    void Append(const T* src, size_t size=-1)
    {
        if ((size_t)-1 == size)
            size = Len(src);
        Vec::Append((T *)src, size);
    }

    void AppendFmt(const T* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        T *res = FmtV(fmt, args);
        AppendAndFree(res);
        va_end(args);
    }

    void AppendAndFree(T* s)
    {
        if (s)
            Append(s, Len(s));
        free(s);
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

// StrVec owns the strings in the list
class StrVec : public Vec<TCHAR *>
{
public:
    StrVec() : Vec() { }
    StrVec(const StrVec& orig) : Vec(orig) {
        // make sure not to share string pointers between StrVecs
        for (size_t i = 0; i < len; i++)
            if (At(i))
                At(i) = str::Dup(At(i));
    }
    ~StrVec() {
        FreeVecMembers(*this);
    }

    StrVec& operator=(const StrVec& that) {
        if (this != &that) {
            FreeVecMembers(*this);
            Vec::operator=(that);
            for (size_t i = 0; i < that.len; i++)
                if (At(i))
                    At(i) = str::Dup(At(i));
        }
        return *this;
    }

    TCHAR *Join(const TCHAR *joint=NULL) {
        str::Str<TCHAR> tmp(256);
        size_t jointLen = joint ? str::Len(joint) : 0;
        for (size_t i = 0; i < len; i++) {
            TCHAR *s = At(i);
            if (i > 0 && jointLen > 0)
                tmp.Append(joint, jointLen);
            tmp.Append(s);
        }
        return tmp.StealData();
    }

    int Find(const TCHAR *string, size_t startAt=0) const {
        for (size_t i = startAt; i < len; i++) {
            TCHAR *item = At(i);
            if (str::Eq(string, item))
                return (int)i;
        }
        return -1;
    }

    /* splits a string into several substrings, separated by the separator
       (optionally collapsing several consecutive separators into one);
       e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
       (resp. "a", "b", "c" if separators are collapsed) */
    size_t Split(const TCHAR *string, const TCHAR *separator, bool collapse=false) {
        size_t start = len;
        const TCHAR *next;

        while ((next = str::Find(string, separator))) {
            if (!collapse || next > string)
                Append(str::DupN(string, next - string));
            string = next + str::Len(separator);
        }
        if (!collapse || *string)
            Append(str::Dup(string));

        return len - start;
    }

    void Sort() { Vec::Sort(cmpAscii); }
    void SortNatural() { Vec::Sort(cmpNatural); }

private:
    static int cmpNatural(const void *a, const void *b) {
        return str::CmpNatural(*(const TCHAR **)a, *(const TCHAR **)b);
    }

    static int cmpAscii(const void *a, const void *b) {
        return _tcscmp(*(const TCHAR **)a, *(const TCHAR **)b);
    }
};

#endif
