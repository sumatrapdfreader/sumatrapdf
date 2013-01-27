/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

/* Simple but also optimized for small sizes vector/array class that can
store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

We always pad the elements with a single 0 value. This makes
Vec<char> and Vec<WCHAR> a C-compatible string. Although it's
not useful for other types, the code is simpler if we always do it
(rather than have it an optional behavior).
*/
template <typename T>
class Vec {
protected:
    static const size_t INTERNAL_BUF_SIZE = 16;
    static const size_t PADDING = 1;

    size_t      len;
    size_t      cap;
    size_t      capacityHint;
    T *         els;
    T           buf[INTERNAL_BUF_SIZE];
    Allocator * allocator;

    // state of the IterStart()/IterNext() iterator
    T *         iterCurr;

    void EnsureCap(size_t needed) {
        if (cap >= needed)
            return;

        size_t newCap = cap * 2;
        if (needed > newCap)
            newCap = needed;
        if (newCap < capacityHint)
            newCap = capacityHint;

        size_t newElCount = newCap + PADDING;
        CrashAlwaysIf(newElCount >= SIZE_MAX / sizeof(T));

        size_t allocSize = newElCount * sizeof(T);
        size_t newPadding = allocSize - len * sizeof(T);
        if (buf == els)
            els = (T *)Allocator::Dup(allocator, buf, len * sizeof(T), newPadding);
        else
            els = (T *)Allocator::Realloc(allocator, els, allocSize);
        CrashAlwaysIf(!els);
        memset(els + len, 0, newPadding);
        cap = newCap;
    }

    T* MakeSpaceAt(size_t idx, size_t count) {
        EnsureCap(len + count);
        T* res = &(els[idx]);
        if (len > idx) {
            T* src = els + idx;
            T* dst = els + idx + count;
            memmove(dst, src, (len - idx) * sizeof(T));
        }
        len += count;
        return res;
    }

    void FreeEls() {
        if (els != buf)
            Allocator::Free(allocator, els);
    }

public:
    // allocator is not owned by Vec and must outlive it
    Vec(size_t capHint=0, Allocator *allocator=NULL) :
        capacityHint(capHint), allocator(allocator), iterCurr(NULL)
    {
        els = buf;
        Reset();
    }

    ~Vec() {
        FreeEls();
    }

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    // note: we don't inherit allocator as it's not needed for our use cases
    Vec(const Vec& orig) : capacityHint(0), allocator(NULL), iterCurr(NULL) {
        els = buf;
        Reset();
        EnsureCap(orig.cap);
        // use memcpy, as Vec only supports POD types
        memcpy(els, orig.els, sizeof(T) * (len = orig.len));
    }

    Vec& operator=(const Vec& that) {
        if (this != &that) {
            EnsureCap(that.cap);
            // use memcpy, as Vec only supports POD types
            memcpy(els, that.els, sizeof(T) * (len = that.len));
            memset(els + len, 0, sizeof(T) * (cap - len));
        }
        return *this;
    }

    T& operator[](size_t idx) {
        CrashIf(idx >= len);
        return els[idx];
    }

    void Reset() {
        len = 0;
        cap = INTERNAL_BUF_SIZE - PADDING;
        FreeEls();
        els = buf;
        memset(buf, 0, sizeof(buf));
    }

    // use &At() if you need a pointer to the element (e.g. if T is a struct)
    T& At(size_t idx) const {
        CrashIf(idx >= len);
        return els[idx];
    }

    T *AtPtr(size_t idx) const {
        CrashIf(idx >= len);
        CrashIf(&els[idx] != &At(idx));
        return &els[idx];
    }

    size_t Count() const {
        return len;
    }

    size_t Size() const {
        return len;
    }

    void InsertAt(size_t idx, const T& el) {
        MakeSpaceAt(idx, 1)[0] = el;
    }

    void Append(const T& el) {
        InsertAt(len, el);
    }

    void Append(const T* src, size_t count) {
        if (0 == count)
            return;
        T* dst = MakeSpaceAt(len, count);
        memcpy(dst, src, count * sizeof(T));
    }

    // appends count blank (i.e. zeroed-out) elements at the end
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

    // This is a fast version of RemoveAt() which replaces the element we're
    // removing with the last element, copying less memory.
    // It can only be used if order of elements doesn't matter and elements
    // can be copied via memcpy()
    // TODO: could be extend to take number of elements to remove
    void RemoveAtFast(size_t idx) {
        CrashIf(idx >= len);
        if (idx >= len) return;
        T *toRemove = els + idx;
        T *last = els + len - 1;
        if (toRemove != last)
            memcpy(toRemove, last, sizeof(T));
        memset(last, 0, sizeof(T));
        --len;
    }

    void Push(T el) {
        Append(el);
    }

    T Pop() {
        CrashIf(0 == len);
        T el = At(len - 1);
        RemoveAt(len - 1);
        return el;
    }

    T& Last() const {
        CrashIf(0 == len);
        return At(len - 1);
    }

    // perf hack for using as a buffer: client can get accumulated data
    // without duplicate allocation. Note: since Vec over-allocates, this
    // is likely to use more memory than strictly necessary, but in most cases
    // it doesn't matter
    T *StealData() {
        T* res = els;
        if (els == buf)
            res = (T *)Allocator::Dup(allocator, buf, (len + PADDING) * sizeof(T));
        els = buf;
        Reset();
        return res;
    }

    T *LendData() const {
        return els;
    }

    int Find(T el, size_t startAt=0) const {
        for (size_t i = startAt; i < len; i++) {
            if (els[i] == el)
                return (int)i;
        }
        return -1;
    }

    // returns true if removed
    bool Remove(T el) {
        int i = Find(el);
        if (-1 == i)
            return false;
        RemoveAt(i);
        return true;
    }

    void Sort(int (*cmpFunc)(const void *a, const void *b)) {
        qsort(els, len, sizeof(T), cmpFunc);
    }

    void Reverse() {
        for (size_t i = 0; i < len / 2; i++) {
            swap(els[i], els[len - i - 1]);
        }
    }

    // Iteration API meant to be used in the following way:
    // for (T *el = vec.IterStart(); el; el = vec.IterNext()) { ... }
    T *IterStart() {
        iterCurr = els;
        return IterNext();
    }

    T *IterNext() {
        T *end = els + len;
        if (end == iterCurr)
            return NULL;
        T *res = iterCurr;
        ++iterCurr;
        return res;
    }

    // return the index of the item returned by last IterStart()/IterNext()
    size_t IterIdx() {
        size_t idx = iterCurr - els;
        CrashIf(0 == idx);
        return idx - 1;
    }
};

// only suitable for T that are pointers that were malloc()ed
template <typename T>
inline void FreeVecMembers(Vec<T>& v)
{
    for (T* el = v.IterStart(); el; el = v.IterNext()) {
        free(*el);
    }
    v.Reset();
}

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(Vec<T>& v)
{
    for (T* el = v.IterStart(); el; el = v.IterNext()) {
        delete *el;
    }
    v.Reset();
}

namespace str {
template <typename T>

class Str : public Vec<T> {
public:
    Str(size_t capHint=0, Allocator *allocator=NULL) : Vec(capHint, allocator) { }

    void Append(T c)
    {
        InsertAt(len, c);
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

// WStrVec owns the strings in the list
class WStrVec : public Vec<WCHAR *>
{
public:
    WStrVec() : Vec() { }
    WStrVec(const WStrVec& orig) : Vec(orig) {
        // make sure not to share string pointers between StrVecs
        for (size_t i = 0; i < len; i++) {
            if (At(i))
                At(i) = str::Dup(At(i));
        }
    }
    ~WStrVec() {
        FreeVecMembers(*this);
    }

    WStrVec& operator=(const WStrVec& that) {
        if (this != &that) {
            FreeVecMembers(*this);
            Vec::operator=(that);
            for (size_t i = 0; i < that.len; i++) {
                if (At(i))
                    At(i) = str::Dup(At(i));
            }
        }
        return *this;
    }

    WCHAR *Join(const WCHAR *joint=NULL) {
        str::Str<WCHAR> tmp(256);
        size_t jointLen = joint ? str::Len(joint) : 0;
        for (size_t i = 0; i < len; i++) {
            WCHAR *s = At(i);
            if (i > 0 && jointLen > 0)
                tmp.Append(joint, jointLen);
            tmp.Append(s);
        }
        return tmp.StealData();
    }

    int Find(const WCHAR *string, size_t startAt=0) const {
        for (size_t i = startAt; i < len; i++) {
            WCHAR *item = At(i);
            if (str::Eq(string, item))
                return (int)i;
        }
        return -1;
    }

    int FindI(const WCHAR *string, size_t startAt=0) const {
        for (size_t i = startAt; i < len; i++) {
            WCHAR *item = At(i);
            if (str::EqI(string, item))
                return (int)i;
        }
        return -1;
    }

    /* splits a string into several substrings, separated by the separator
       (optionally collapsing several consecutive separators into one);
       e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
       (resp. "a", "b", "c" if separators are collapsed) */
    size_t Split(const WCHAR *string, const WCHAR *separator, bool collapse=false) {
        size_t start = len;
        const WCHAR *next;

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
        return str::CmpNatural(*(const WCHAR **)a, *(const WCHAR **)b);
    }

    static int cmpAscii(const void *a, const void *b) {
        return wcscmp(*(const WCHAR **)a, *(const WCHAR **)b);
    }
};

// WStrList is a subset of WStrVec that's optimized for appending and searching
// WStrList owns the strings it contains and frees them at destruction
class WStrList {
    struct Item {
        WCHAR *string;
        uint32_t hash;
    };

    Vec<Item> items;
    size_t count;
    Allocator *allocator;

    // variation of CRC-32 which deals with strings that are
    // mostly ASCII and should be treated case independently
    static uint32_t GetQuickHashI(const WCHAR *str) {
        uint32_t crc = 0;
        for (; *str; str++) {
            uint32_t bits = (crc ^ ((towlower((wint_t)*str) & 0xFF) << 24)) & 0xFF000000L;
            for (int i = 0; i < 8; i++) {
                if ((bits & 0x80000000L))
                    bits = (bits << 1) ^ 0x04C11DB7L;
                else
                    bits <<= 1;
            }
            crc = (crc << 8) ^ bits;
        }
        return crc;
    }

public:
    WStrList(size_t capHint=0, Allocator *allocator=NULL) :
        items(capHint, allocator), count(0), allocator(allocator) { }

    ~WStrList() {
        for (Item *item = items.IterStart(); item; item = items.IterNext()) {
            Allocator::Free(allocator, item->string);
        }
    }

    const WCHAR *At(size_t idx) const {
        return items.At(idx).string;
    }

    size_t Count() const {
        return count;
    }

    // str must have been allocated by allocator and is owned by StrList
    void Append(WCHAR *str) {
        Item item = { str, GetQuickHashI(str) };
        items.Append(item);
        count++;
    }

    int Find(const WCHAR *str, size_t startAt=0) const {
        uint32_t hash = GetQuickHashI(str);
        Item *item = items.LendData();
        for (size_t i = startAt; i < count; i++) {
            if (item[i].hash == hash && str::Eq(item[i].string, str))
                return (int)i;
        }
        return -1;
    }

    int FindI(const WCHAR *str, size_t startAt=0) const {
        uint32_t hash = GetQuickHashI(str);
        Item *item = items.LendData();
        for (size_t i = startAt; i < count; i++) {
            if (item[i].hash == hash && str::EqI(item[i].string, str))
                return (int)i;
        }
        return -1;
    }
};
