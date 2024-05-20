/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// represents null string
constexpr u32 kNullOffset = (u32)-2;

void StrVec::Reset() {
    strings.Reset();
    offsets.Reset();
}

// returns index of inserted string
int StrVec::Append(const char* s, int sLen) {
    bool ok;
    if (s == nullptr) {
        ok = offsets.Append(kNullOffset);
        if (!ok) {
            return -1;
        }
        return Size() - 1;
    }
    if (sLen < 0) {
        sLen = str::Leni(s);
    }
    u32 idx = (u32)strings.Size();
    ok = strings.Append(s, (size_t)sLen);
    // ensure to always zero-terminate
    ok &= strings.AppendChar(0);
    if (!ok) {
        return -1;
    }
    ok = offsets.Append(idx);
    if (!ok) {
        return -1;
    }
    return Size() - 1;
}

// returns index of inserted string, -1 if not inserted
int StrVec::AppendIfNotExists(const char* s) {
    if (Contains(s)) {
        return -1;
    }
    return Append(s);
}

bool StrVec::InsertAt(int idx, const char* s) {
    size_t n = str::Len(s);
    u32 strIdx = (u32)strings.size();
    bool ok = strings.Append(s, n + 1); // also append terminating 0
    if (!ok) {
        return false;
    }
    return offsets.InsertAt(idx, strIdx);
}

char* StrVec::SetAt(int idx, const char* s) {
    if (s == nullptr) {
        offsets[idx] = kNullOffset;
        return nullptr;
    }
    size_t n = str::Len(s);
    u32 strIdx = (u32)strings.size();
    bool ok = strings.Append(s, n + 1); // also append terminating 0
    if (!ok) {
        return nullptr;
    }
    offsets[idx] = strIdx;
    return strings.Get() + strIdx;
}

int StrVec::Size() const {
    return offsets.Size();
}

char* StrVec::operator[](int idx) const {
    CrashIf(idx < 0);
    return At(idx);
}

char* StrVec::At(int idx) const {
    int n = Size();
    CrashIf(idx < 0 || idx >= n);
    u32 start = offsets.at(idx);
    if (start == kNullOffset) {
        return nullptr;
    }
    char* s = strings.LendData() + start;
    return s;
}

int StrVec::Find(const char* sv, int startAt) const {
    int n = Size();
    for (int i = startAt; i < n; i++) {
        auto s = At(i);
        if (str::Eq(sv, s)) {
            return i;
        }
    }
    return -1;
}

int StrVec::FindI(const char* sv, int startAt) const {
    int n = Size();
    for (int i = startAt; i < n; i++) {
        auto s = At(i);
        if (str::EqI(sv, s)) {
            return i;
        }
    }
    return -1;
}

bool StrVec::Contains(const char* s) const {
    int idx = Find(s);
    return idx != -1;
}

// Note: adding might invalidate the returned string due to re-allocation
// of underlying strings memory
char* StrVec::RemoveAt(int idx) {
    u32 strOff = offsets[idx];
    offsets.RemoveAt(idx);
    char* res = (strOff == kNullOffset) ? nullptr : strings.Get() + strOff;
    return res;
}

// Note: returned string remains valid as long as StrVec is valid
char* StrVec::RemoveAtFast(int idx) {
    CrashIf(idx < 0);
    u32 strOff = offsets[idx];
    offsets.RemoveAtFast((size_t)idx);
    char* res = (strOff == kNullOffset) ? nullptr : strings.Get() + strOff;
    return res;
}

// return true if did remove
bool StrVec::Remove(const char* s) {
    int idx = Find(s);
    if (idx >= 0) {
        RemoveAt(idx);
        return true;
    }
    return false;
}

StrVec::iterator StrVec::begin() const {
    return StrVec::iterator(this, 0);
}
StrVec::iterator StrVec::end() const {
    return StrVec::iterator(this, this->Size());
}

StrVec::iterator::iterator(const StrVec* v, int idx) {
    this->v = v;
    this->idx = idx;
}

char* StrVec::iterator::operator*() const {
    return v->At(idx);
}

StrVec::iterator& StrVec::iterator::operator++() {
    idx++;
    return *this;
}

StrVec::iterator& StrVec::iterator::operator++(int) {
    idx++;
    return *this;
}

StrVec::iterator& StrVec::iterator::operator+(int n) {
    idx += n;
    return *this;
}

bool operator==(const StrVec::iterator& a, const StrVec::iterator& b) {
    return a.idx == b.idx;
};

bool operator!=(const StrVec::iterator& a, const StrVec::iterator& b) {
    return a.idx != b.idx;
};

static bool strLess(const char* s1, const char* s2) {
    if (str::IsEmpty(s1)) {
        if (str::IsEmpty(s2)) {
            return false;
        }
        return true;
    }
    if (str::IsEmpty(s2)) {
        return false;
    }
    int n = strcmp(s1, s2);
    return n < 0;
}

static bool strLessNoCase(const char* s1, const char* s2) {
    if (str::IsEmpty(s1)) {
        // null / empty string is smallest
        if (str::IsEmpty(s2)) {
            return false;
        }
        return true;
    }
    if (str::IsEmpty(s2)) {
        return false;
    }
    int n = _stricmp(s1, s2);
    return n < 0;
}

static bool strLessNatural(const char* s1, const char* s2) {
    int n = str::CmpNatural(s1, s2);
    return n < 0; // TODO: verify it's < and not >
}

void Sort(StrVec& v, StrLessFunc lessFn) {
    if (lessFn == nullptr) {
        lessFn = strLess;
    }
    const char* strs = v.strings.Get();
    auto b = v.offsets.begin();
    auto e = v.offsets.end();
    std::sort(b, e, [strs, lessFn](u32 i1, u32 i2) -> bool {
        const char* s1 = (i1 == kNullOffset) ? nullptr : strs + i1;
        const char* s2 = (i2 == kNullOffset) ? nullptr : strs + i2;
        bool ret = lessFn(s1, s2);
        return ret;
    });
}

void SortNoCase(StrVec& v) {
    Sort(v, strLessNoCase);
}

void SortNatural(StrVec& v) {
    Sort(v, strLessNatural);
}

/* splits a string into several substrings, separated by the separator
(optionally collapsing several consecutive separators into one);
e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
(resp. "a", "b", "c" if separators are collapsed) */
template <typename StrVec>
int SplitT(StrVec& v, const char* s, const char* separator, bool collapse) {
    int startSize = v.Size();
    const char* next;
    while (true) {
        next = str::Find(s, separator);
        if (!next) {
            break;
        }
        if (!collapse || next > s) {
            int sLen = (int)(next - s);
            v.Append(s, sLen);
        }
        s = next + str::Len(separator);
    }
    if (!collapse || *s) {
        v.Append(s);
    }

    return (size_t)(v.Size() - startSize);
}

int Split(StrVec& v, const char* s, const char* separator, bool collapse) {
    return SplitT<StrVec>(v, s, separator, collapse);
}

template <typename StrVec>
static int CalcCapForJoin(const StrVec& v, const char* joint) {
    // it's ok to over-estimate
    int len = v.Size();
    size_t jointLen = str::Len(joint);
    int cap = len * (int)jointLen;
    for (int i = 0; i < len; i++) {
        char* s = v.At(i);
        cap += (int)str::Len(s);
    }
    return cap + 32; // +32 arbitrary buffer
}

template <typename StrVec>
static char* JoinInner(const StrVec& v, const char* joint, str::Str& res) {
    int len = v.Size();
    size_t jointLen = str::Len(joint);
    int firstForJoint = 0;
    for (int i = 0; i < len; i++) {
        char* s = v.At(i);
        if (!s) {
            firstForJoint++;
            continue;
        }
        if (i > firstForJoint && jointLen > 0) {
            res.Append(joint, jointLen);
        }
        res.Append(s);
    }
    return res.StealData();
}

char* Join(const StrVec& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint);
    return JoinInner(v, joint, tmp);
}

TempStr JoinTemp(const StrVec& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint, GetTempAllocator());
    return JoinInner(v, joint, tmp);
}

ByteSlice ToByteSlice(const char* s) {
    size_t n = str::Len(s);
    return {(u8*)s, n};
}

/*
    TODO:
    - StrVecWithData where it associate arbitrary data with each string
    - StrVecWithSubset - has additional index which contains a subset
    of strings which we create by providing a filter function.
    Could be used for efficiently managing strings in
    Command Palette
*/

struct StrVecWithDataRaw {
    str::Str strings;
    Vec<u32> index;
    Vec<uintptr_t> data;

    StrVecWithDataRaw() = default;
    ~StrVecWithDataRaw() = default;

    int Size() const;
    char* at(int) const;
    char* operator[](int) const;

    int Append(const char*, uintptr_t);
    uintptr_t GetData(int) const;
};

int StrVecWithDataRaw::Size() const {
    int n = index.Size();
    CrashIf(data.Size() != n);
    return n;
}

char* StrVecWithDataRaw::at(int idx) const {
    int n = Size();
    CrashIf(idx < 0 || idx >= n);
    u32 start = index.at(idx);
    if (start == kNullOffset) {
        return nullptr;
    }
    char* s = strings.LendData() + start;
    return s;
}

char* StrVecWithDataRaw::operator[](int idx) const {
    return at(idx);
}

int StrVecWithDataRaw::Append(const char*, uintptr_t) {
    return -1;
}

uintptr_t StrVecWithDataRaw::GetData(int idx) const {
    int n = Size();
    CrashIf(idx < 0 || idx >= n);
    uintptr_t res = data.at(idx);
    return res;
}

template <typename T>
struct StrVecWithData : StrVecWithDataRaw {
    int Append(const char* s, T data) {
        uintptr_t d = (uintptr_t)data;
        return StrVecWithDataRaw::Append(s, d);
    }
    T GetData(int idx) const {
        uintptr_t res = StrVecWithDataRaw::GetData(idx);
        return (T)(res);
    }
};

// TODO: support strings with 0 in them by storing size of the string
struct StrVecPage {
    struct StrVecPage* next;
    int pageSize;
    int nStrings;
    char* currEnd;
    // now follows u32[nStrings] offsets since the beginning of StrVecPage

    // strings are allocated from the end

    char* At(int) const;
    StrSpan AtSpan(int i) const;
    char* RemoveAt(int);
    char* RemoveAtFast(int);

    char* AtHelper(int, int& sLen) const;
    int BytesLeft();
    char* Append(const char* s, int len);
    char* SetAt(const char* s, int len, int idxSet);
};

constexpr int kStrVecPageHdrSize = (int)sizeof(StrVecPage);

static int cbOffsetsSize(int nStrings) {
    return nStrings * 2 * sizeof(u32);
}

int StrVecPage::BytesLeft() {
    char* start = (char*)this;
    start += kStrVecPageHdrSize;
    int cbTotal = (int)(currEnd - start);
    int cbOffsets = cbOffsetsSize(nStrings);
    auto res = cbTotal - cbOffsets;
    CrashIf(res < 0);
    return res;
}

static StrVecPage* AllocStrVecPage(int pageSize) {
    auto page = (StrVecPage*)Allocator::AllocZero(nullptr, pageSize);
    page->next = nullptr;
    page->nStrings = 0;
    page->pageSize = pageSize;
    char* start = (char*)page;
    page->currEnd = start + pageSize;
    return page;
}

#define kNoSpace (char*)-2

u32* OffsetsForString(const StrVecPage* p, int idx) {
    CrashIf(idx < 0 || idx >= p->nStrings);
    char* start = (char*)p;
    u32* offsets = (u32*)(start + kStrVecPageHdrSize);
    return offsets + (idx * 2);
}

// if idx < 0 we append, otherwise we replace
char* StrVecPage::SetAt(const char* s, int sLen, int idx) {
    u32* offsets = OffsetsForString(this, idx);
    if (!s) {
        // fast path for null, doesn't require new space at all
        offsets[0] = kNullOffset;
        offsets[1] = 0;
        return nullptr;
    }
    u32 off = offsets[0];
    char* start = (char*)this;
    if (off != kNullOffset) {
        // fast path for when new string is smaller than the current string
        int currLen = (int)offsets[1];
        if (sLen <= currLen) {
            auto curr = start + off;
            memcpy(curr, s, (size_t)sLen);
            curr[sLen] = 0; // zero-terminate for C compat
            return curr;
        }
    }

    int cbNeeded = 0;     // when replacing, we re-use offset / size slots
    int cbStr = sLen + 1; // +1 for zero termination
    if (s) {
        cbNeeded += cbStr;
    }

    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return kNoSpace;
    }

    currEnd -= cbStr;
    off = (u32)(currEnd - start);
    offsets[0] = (u32)off;
    offsets[1] = (u32)sLen;
    memcpy(currEnd, s, (size_t)sLen);
    currEnd[sLen] = 0; // zero-terminate for C compat
    return currEnd;
}

// if idx < 0 we append, otherwise we replace
char* StrVecPage::Append(const char* s, int sLen) {
    int cbNeeded = sizeof(u32) * 2; // for offset / size
    int cbStr = sLen + 1;           // +1 for zero termination
    if (s) {
        cbNeeded += cbStr;
    }
    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return kNoSpace;
    }
    int idx = nStrings++;
    u32* offsets = OffsetsForString(this, idx);
    if (!s) {
        offsets[0] = kNullOffset;
        offsets[1] = 0;
        return nullptr;
    }

    currEnd -= cbStr;
    size_t off = (currEnd - (char*)this);
    offsets[0] = (u32)off;
    offsets[1] = (u32)sLen;
    memcpy(currEnd, s, (size_t)sLen);
    currEnd[sLen] = 0; // zero-terminate for C compat
    return currEnd;
}

char* StrVecPage::AtHelper(int idx, int& sLen) const {
    u8* start = (u8*)this;
    u32* offsets = OffsetsForString(this, idx);
    u32 off = offsets[0];
    sLen = (int)offsets[1];
    if (off == kNullOffset) {
        return nullptr;
    }
    char* s = (char*)(start + off);
    return s;
}

char* StrVecPage::At(int idx) const {
    int sLen;
    char* s = AtHelper(idx, sLen);
    return s;
}

StrSpan StrVecPage::AtSpan(int idx) const {
    int sLen;
    char* s = AtHelper(idx, sLen);
    return {s, sLen};
}

// we don't de-allocate removed strings so we can safely return the string
char* StrVecPage::RemoveAt(int idx) {
    int sLen;
    char* s = AtHelper(idx, sLen);
    nStrings--;
    int nToCopy = cbOffsetsSize(nStrings - idx);
    if (nToCopy == 0) {
        // last string
        // TODO(perf): removing the last string
        // if currEnd is offset of this string, push it forward by sLen
        // to free the space used by it
        return s;
    }
    u32* dst = OffsetsForString(this, idx);
    u32* src = dst + 2;
    memmove((void*)dst, (void*)src, nToCopy);
    return s;
}

char* StrVecPage::RemoveAtFast(int idx) {
    int sLen;
    char* s = AtHelper(idx, sLen);
    // TODO(perf): if removing the last string
    // if currEnd is offset of this string, push it forward by sLen
    // to free the space used by it
    u32* dst = OffsetsForString(this, idx);
    u32* src = OffsetsForString(this, nStrings - 1);
    *dst++ = *src++;
    *dst = *src;
    nStrings--;
    return s;
}

static void FreePages(StrVecPage* toFree) {
    StrVecPage* next;
    while (toFree) {
        next = toFree->next;
        Allocator::Free(nullptr, toFree);
        toFree = next;
    }
}

static StrVecPage* CompactStrVecPages(StrVecPage* first, int extraSize) {
    if (!first) {
        CrashIf(extraSize > 0);
        return nullptr;
    }
    int nStrings = 0;
    int cbStringsTotal = 0; // including 0-termination
    auto curr = first;
    char *pageStart, *pageEnd;
    int cbStrings;
    while (curr) {
        nStrings += curr->nStrings;
        pageStart = (char*)curr;
        pageEnd = pageStart + curr->pageSize;
        cbStrings = (int)(pageEnd - curr->currEnd);
        cbStringsTotal += cbStrings;
        curr = curr->next;
    }
    // strLenTotal might be more than needed if we removed strings, but that's ok
    int pageSize = kStrVecPageHdrSize + cbOffsetsSize(nStrings) + cbStringsTotal;
    if (extraSize > 0) {
        pageSize += extraSize;
    }
    pageSize = RoundUp(pageSize, 64); // jic
    auto page = AllocStrVecPage(pageSize);
    int n;
    StrSpan s;
    curr = first;
    while (curr) {
        n = curr->nStrings;
        // TODO(perf): could optimize slightly
        for (int i = 0; i < n; i++) {
            s = curr->AtSpan(i);
            page->Append(s.Str(), s.Len());
        }
        curr = curr->next;
    }
    return page;
}

static void CompactPages(StrVec2* v, int extraSize) {
    auto first = CompactStrVecPages(v->first, extraSize);
    FreePages(v->first);
    v->first = first;
    v->curr = first;
    CrashIf(first && (v->size != first->nStrings));
}

void StrVec2::Reset(StrVecPage* initWith) {
    FreePages(first);
    first = nullptr;
    curr = nullptr;
    nextPageSize = 256; // TODO: or leave it alone?
    size = 0;
    if (initWith == nullptr) {
        return;
    }
    first = CompactStrVecPages(initWith, 0);
    curr = first;
    size = first->nStrings;
}

StrVec2::~StrVec2() {
    Reset(nullptr);
}

StrVec2::StrVec2(const StrVec2& that) {
    Reset(that.first);
}

StrVec2& StrVec2::operator=(const StrVec2& that) {
    if (this == &that) {
        return *this;
    }
    Reset(that.first);
    return *this;
}

#if 0
static void UpdateSize(StrVec2* v) {
    int n = 0;
    auto page = v->first;
    while (page) {
        n += page->nStrings;
        page = page->next;
    }
    v->cachedSize = n;
}
#endif

int StrVec2::Size() const {
    return size;
}

static int CalcNextPageSize(int currSize) {
    // at the beginning grow faster
    if (currSize == 256) {
        return 1024;
    }
    if (currSize == 1024) {
        return 4 * 1024;
    }
    if (currSize >= 64 * 1024) {
        // cap the page size at 64 kB
        return currSize;
    }
    return currSize * 2;
}

char* StrVec2::Append(const char* s, int sLen) {
    if (sLen < 0) {
        sLen = str::Leni(s);
    }
    int nBytesNeeded = sizeof(u32) * 2; // for index and size
    if (s) {
        nBytesNeeded += (sLen + 1); // +1 for zero termination
    }
    if (!curr || curr->BytesLeft() < nBytesNeeded) {
        int minPageSize = kStrVecPageHdrSize + nBytesNeeded;
        int pageSize = RoundUp(minPageSize, 8);
        if (pageSize < nextPageSize) {
            pageSize = nextPageSize;
            nextPageSize = CalcNextPageSize(nextPageSize);
        }
        auto page = AllocStrVecPage(pageSize);
        if (curr) {
            CrashIf(!first);
            curr->next = page;
        } else {
            CrashIf(first);
            first = page;
        }
        curr = page;
    }
    auto res = curr->Append(s, sLen);
    size++;
    return res;
}

static std::pair<StrVecPage*, int> PageForIdx(const StrVec2* v, int idx) {
    auto page = v->first;
    while (page) {
        if (page->nStrings > idx) {
            return {page, idx};
        }
        idx -= page->nStrings;
        page = page->next;
    }
    return {nullptr, 0};
}

char* StrVec2::At(int idx) const {
    auto [page, idxInPage] = PageForIdx(this, idx);
    return page->At(idxInPage);
}

StrSpan StrVec2::AtSpan(int idx) const {
    auto [page, idxInPage] = PageForIdx(this, idx);
    return page->AtSpan(idxInPage);
}

char* StrVec2::operator[](int idx) const {
    CrashIf(idx < 0);
    return At(idx);
}

int StrVec2::Find(const char* s, int startAt) const {
    auto end = this->end();
    for (auto it = this->begin() + startAt; it != end; it++) {
        char* s2 = *it;
        if (str::Eq(s, s2)) {
            return it.idx;
        }
    }
    return -1;
}

int StrVec2::FindI(const char* s, int startAt) const {
    auto end = this->end();
    for (auto it = this->begin() + startAt; it != end; it++) {
        // TODO(perf): check length firsts
        char* s2 = *it;
        if (str::EqI(s, s2)) {
            return it.idx;
        }
    }
    return -1;
}

// returns a string
// note: this might invalidate previously returned strings because
// it might re-allocate memore used for those strings
char* StrVec2::SetAt(int idx, const char* s, int sLen) {
    {
        auto [page, idxInPage] = PageForIdx(this, idx);
        if (sLen < 0) {
            sLen = str::Leni(s);
        }
        char* res = page->SetAt(s, sLen, idxInPage);
        if (res != kNoSpace) {
            return res;
        }
    }
    // perf: we assume that there will be more SetAt() calls so pre-allocate
    // extra space to make many SetAt() calls less expensive
    int extraSpace = RoundUp(sLen + 1, 2048);
    CompactPages(this, extraSpace);
    char* res = first->SetAt(s, sLen, idx);
    CrashIf(res == kNoSpace);
    return res;
}

// remove string at idx and return it
// return value is valid as long as StrVec2 is valid
char* StrVec2::RemoveAt(int idx) {
    auto [page, idxInPage] = PageForIdx(this, idx);
    auto res = page->RemoveAt(idxInPage);
    size--;
    return res;
}

// remove string at idx more quickly but will change order of string
// return value is valid as long as StrVec2 is valid
char* StrVec2::RemoveAtFast(int idx) {
    auto [page, idxInPage] = PageForIdx(this, idx);
    auto res = page->RemoveAtFast(idxInPage);
    size--;
    return res;
}

StrVec2::iterator StrVec2::begin() const {
    return StrVec2::iterator(this, 0);
}

StrVec2::iterator StrVec2::end() const {
    return StrVec2::iterator(this, this->Size());
}

StrVec2::iterator::iterator(const StrVec2* v, int idx) {
    this->v = v;
    this->idx = idx;
    auto [page, idxInPage] = PageForIdx(v, idx);
    this->page = page;
    this->idxInPage = idxInPage;
}

char* StrVec2::iterator::operator*() const {
    return page->At(idxInPage);
}

static void Next(StrVec2::iterator& it, int n) {
    // TODO: optimize for n > 1
    for (int i = 0; i < n; i++) {
        it.idx++;
        it.idxInPage++;
        if (it.idxInPage >= it.page->nStrings) {
            it.idxInPage = 0;
            it.page = it.page->next;
        }
    }
}

StrVec2::iterator& StrVec2::iterator::operator++(int) {
    Next(*this, 1);
    return *this;
}

StrVec2::iterator& StrVec2::iterator::operator++() {
    Next(*this, 1);
    return *this;
}

StrVec2::iterator& StrVec2::iterator::operator+(int n) {
    Next(*this, n);
    return *this;
}

bool operator==(const StrVec2::iterator& a, const StrVec2::iterator& b) {
    return a.idx == b.idx;
};

bool operator!=(const StrVec2::iterator& a, const StrVec2::iterator& b) {
    return a.idx != b.idx;
};

void Sort(StrVec2& v, StrLessFunc lessFn) {
    if (!v.first) {
        return;
    }
    if (lessFn == nullptr) {
        lessFn = strLess;
    }
    CompactPages(&v, 0);
    if (v.Size() == 0) {
        return;
    }
    CrashIf(!v.first);
    CrashIf(v.first->next);
    int n = v.Size();

    const char* pageStart = (const char*)v.first;
    u64* b = (u64*)(pageStart + kStrVecPageHdrSize);
    u64* e = b + n;
    std::sort(b, e, [pageStart, lessFn](u64 offLen1, u64 offLen2) -> bool {
        u32 off1 = (u32)(offLen1 & 0xffffffff);
        u32 off2 = (u32)(offLen2 & 0xffffffff);
        const char* s1 = (off1 == kNullOffset) ? nullptr : pageStart + off1;
        const char* s2 = (off2 == kNullOffset) ? nullptr : pageStart + off2;
        bool ret = lessFn(s1, s2);
        return ret;
    });
}

void SortNoCase(StrVec2& v) {
    Sort(v, strLessNoCase);
}

void SortNatural(StrVec2& v) {
    Sort(v, strLessNatural);
}

int Split(StrVec2& v, const char* s, const char* separator, bool collapse) {
    return SplitT<StrVec2>(v, s, separator, collapse);
}

char* Join(StrVec2& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint);
    return JoinInner(v, joint, tmp);
}

TempStr JoinTemp(StrVec2& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint, GetTempAllocator());
    return JoinInner(v, joint, tmp);
}
