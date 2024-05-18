/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// represents null string
constexpr u32 kNullIdx = (u32)-2;

void StrVec::Reset() {
    strings.Reset();
    index.Reset();
}

// returns index of inserted string
int StrVec::Append(const char* s, int sLen) {
    bool ok;
    if (s == nullptr) {
        ok = index.Append(kNullIdx);
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
    ok = index.Append(idx);
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
    return index.InsertAt(idx, strIdx);
}

char* StrVec::SetAt(int idx, const char* s) {
    if (s == nullptr) {
        index[idx] = kNullIdx;
        return nullptr;
    }
    size_t n = str::Len(s);
    u32 strIdx = (u32)strings.size();
    bool ok = strings.Append(s, n + 1); // also append terminating 0
    if (!ok) {
        return nullptr;
    }
    index[idx] = strIdx;
    return strings.Get() + strIdx;
}

int StrVec::Size() const {
    return index.Size();
}

char* StrVec::operator[](int idx) const {
    CrashIf(idx < 0);
    return At(idx);
}

char* StrVec::At(int idx) const {
    int n = Size();
    CrashIf(idx < 0 || idx >= n);
    u32 start = index.at(idx);
    if (start == kNullIdx) {
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
    u32 strIdx = index[idx];
    index.RemoveAt(idx);
    char* res = (strIdx == kNullIdx) ? nullptr : strings.Get() + strIdx;
    return res;
}

// Note: returned string remains valid as long as StrVec is valid
char* StrVec::RemoveAtFast(int idx) {
    CrashIf(idx < 0);
    u32 strIdx = index[idx];
    index.RemoveAtFast((size_t)idx);
    char* res = (strIdx == kNullIdx) ? nullptr : strings.Get() + strIdx;
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
    auto b = v.index.begin();
    auto e = v.index.end();
    std::sort(b, e, [strs, lessFn](u32 i1, u32 i2) -> bool {
        const char* s1 = (i1 == kNullIdx) ? nullptr : strs + i1;
        const char* s2 = (i2 == kNullIdx) ? nullptr : strs + i2;
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
int Split(StrVec& v, const char* s, const char* separator, bool collapse) {
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

static int CalcCapForJoin(const StrVec& v, const char* joint) {
    // it's ok to over-estimate
    int len = v.Size();
    size_t jointLen = str::Len(joint);
    int cap = len * (int)jointLen;
    for (int i = 0; i < len; i++) {
        char* s = v.At(i);
        cap += (int)str::Len(s);
    }
    return cap + 32; // arbitrary buffer
}

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
    if (start == kNullIdx) {
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
    char* RemoveAt(int);
    char* RemoveAtFast(int);
    char* SetAt(int idx, const char* s, int sLen);

    char* AtHelper(int, u32*&) const;
    int BytesLeft();
    char* Append(const char* s, int len, int idxSet = -1);
};

constexpr int kStrVecPageHdrSize = (int)sizeof(StrVecPage);

int StrVecPage::BytesLeft() {
    char* start = (char*)this;
    start += kStrVecPageHdrSize;
    int nTotal = (int)(currEnd - start);
    int nOffsets = nStrings * sizeof(u32);
    return nTotal - nOffsets;
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

char* StrVecPage::Append(const char* s, int sLen, int idxToSet) {
    bool append = idxToSet < 0;                  // otherwise replace
    int nBytesNeeded = append ? sizeof(u32) : 0; // for index
    int nBytes = sLen + 1;                       // +1 for zero termination
    if (s) {
        nBytesNeeded += nBytes;
    }
    if (append) {
        idxToSet = nStrings;
    }
    int nBytesLeft = BytesLeft();
    if (nBytesNeeded > nBytesLeft) {
        return kNoSpace;
    }
    if (append) {
        nStrings++;
    }
    char* start = (char*)this;
    u32* offsets = (u32*)(start + kStrVecPageHdrSize);
    if (!s) {
        offsets[idxToSet] = kNullIdx;
        return nullptr;
    }
    currEnd -= nBytes;
    size_t offset = (currEnd - start);
    offsets[idxToSet] = (u32)offset;
    memcpy(currEnd, s, (size_t)sLen);
    currEnd[sLen] = 0; // zero-terminate for C compat
    return currEnd;
}

char* StrVecPage::AtHelper(int idx, u32*& offsets) const {
    CrashIf(idx < 0 || idx >= nStrings);
    u8* start = (u8*)this;
    offsets = (u32*)(start + kStrVecPageHdrSize);
    u32 offset = offsets[idx];
    if (offset == kNullIdx) {
        return nullptr;
    }
    char* s = (char*)(start + offset);
    return s;
}

char* StrVecPage::At(int idx) const {
    u32* offsets;
    char* s = AtHelper(idx, offsets);
    return s;
}

// we don't de-allocate removed strings so we can safely return the string
char* StrVecPage::RemoveAt(int idx) {
    u32* offsets;
    char* s = AtHelper(idx, offsets);
    nStrings--;

    int nToCopy = (nStrings - idx) * sizeof(u32);
    if (nToCopy == 0) {
        return s;
    }
    u32* dst = offsets + idx;
    u32* src = offsets + idx + 1;
    memmove((void*)dst, (void*)src, nToCopy);
    return s;
}

char* StrVecPage::RemoveAtFast(int idx) {
    u32* offsets;
    char* s = AtHelper(idx, offsets);
    nStrings--;

    int lastIdx = nStrings;
    offsets[idx] = offsets[lastIdx];
    return s;
}

char* StrVecPage::SetAt(int idx, const char* s, int sLen) {
    char* res = Append(s, sLen, idx);
    return res;
    if (res != kNoSpace) {
        return res;
    }
    // TODO: have to resize page
    CrashIf(true);
    return nullptr;
}

static void FreePages(StrVecPage* toFree) {
    StrVecPage* next;
    while (toFree) {
        next = toFree->next;
        Allocator::Free(nullptr, toFree);
        toFree = next;
    }
}

static StrVecPage* CompactPages(StrVecPage* first) {
    if (!first) {
        return nullptr;
    }
    if (!first->next) {
        // if only one, no need to compact
        return first;
    }
    int nStrings = 0;
    int strLenTotal = 0; // including 0-termination
    auto curr = first;
    char *pageStart, *pageEnd;
    int strsLen;
    while (curr) {
        nStrings += curr->nStrings;
        pageStart = (char*)curr;
        pageEnd = pageStart + curr->pageSize;
        strsLen = (int)(pageEnd - curr->currEnd);
        strLenTotal += strsLen;
        curr = curr->next;
    }
    // strLenTotal might be more than needed if we removed strings, but that's ok
    int pageSize = kStrVecPageHdrSize + (nStrings * sizeof(u32)) + strLenTotal;
    pageSize = RoundUp(pageSize, 64); // just in case
    auto page = AllocStrVecPage(pageSize);
    curr = first;
    int i, n, sLen;
    char* s;
    while (curr) {
        n = curr->nStrings;
        for (i = 0; i < n; i++) {
            s = curr->At(i);
            sLen = str::Leni(s);
            page->Append(s, sLen, -1);
        }
        curr = curr->next;
    }
    FreePages(first);
    return page;
}

static void CompactPages(StrVec2* v) {
    auto first = CompactPages(v->first);
    v->first = first;
    v->curr = first;
    CrashIf(!first && (v->cachedSize != first->nStrings));
}

struct SideString {
    SideString* next;
    int idx;
    char* s; // just for debugger, should be <start of SideStrings> + sizeof(SideStrings)
    // string data followss
};

SideString* AllocSideString(int idx, const char* s, int sLen) {
    int n = sLen + 1 + sizeof(SideString);
    auto ss = (SideString*)Allocator::Alloc(nullptr, (size_t)sLen);
    ss->next = 0;
    ss->idx = idx;
    char* s2 = (char*)ss;
    s2 += sizeof(SideString);
    ss->s = s2;
    memmove(s2, s, sLen);
    s2[sLen] = 0;
    return ss;
}

void FreeSideStrings(SideString* curr) {
    while (curr) {
        SideString* next = curr->next;
        Allocator::Free(nullptr, (void*)curr);
        curr = next;
    }
}

// return true if removed a string at idx
bool SideStringsRemoveAt(SideString** currPtr, int idx) {
    if (!*currPtr) {
        return false;
    }
    int n = 0;
    auto curr = *currPtr;
    bool needRemove = false;
    while (curr) {
        if (curr->idx == idx) {
            needRemove = true;
        }
        n++;
    }
    if (!needRemove) {
        return false;
    }
    auto v = Vec<SideString*>(n);
    curr = *currPtr;
    while (curr) {
        if (curr->idx != idx) {
            v.Append(curr);
        }
        curr = curr->next;
    }
    curr = v[0];
    curr->next = nullptr;
    *currPtr = curr;
    n = v.Size();
    auto prev = curr;
    for (int i = 1; i < n; i++) {
        curr = v[i];
        curr->next = nullptr;
        prev->next = curr;
    }
    return false;
}

void StrVec2::Reset() {
    FreePages(first);
    FreeSideStrings(firstSide);
    first = nullptr;
    curr = nullptr;
    firstSide = nullptr;
    nextPageSize = 256; // TODO: or leave it alone?
    cachedSize = 0;
}

StrVec2::~StrVec2() {
    Reset();
}

StrVec2::StrVec2(const StrVec2& that) {
    // TODO: unoptimized, should compact into a single StrVecPage
    Reset();
    int n = that.Size();
    for (int i = 0; i < n; i++) {
        char* s = that.At(i);
        Append(s);
    }
}

StrVec2& StrVec2::operator=(const StrVec2& that) {
    if (this == &that) {
        return *this;
    }
    Reset();
    int n = that.Size();
    for (int i = 0; i < n; i++) {
        char* s = that.At(i);
        Append(s);
    }
    return *this;
}

static void UpdateSize(StrVec2* v) {
    int n = 0;
    auto page = v->first;
    while (page) {
        n += page->nStrings;
        page = page->next;
    }
    v->cachedSize = n;
}

int StrVec2::Size() const {
    return cachedSize;
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
    int nBytesNeeded = sizeof(32); // for index
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
    UpdateSize(this);
    return res;
}

static StrVecPage* PageForIdx(const StrVec2* v, int& idx) {
    auto page = v->first;
    while (page) {
        if (page->nStrings > idx) {
            return page;
        }
        idx -= page->nStrings;
        page = page->next;
    }
    return nullptr;
}

static char* SideStringAt(SideString* first, int idx) {
    while (first) {
        if (first->idx == idx) {
            return first->s;
        }
        first = first->next;
    }
    return nullptr;
}

char* StrVec2::At(int idx) const {
    char* s = SideStringAt(firstSide, idx);
    if (s) {
        return s;
    }
    int idxInPage = idx;
    auto page = PageForIdx(this, idxInPage);
    return page->At(idxInPage);
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
        char* s2 = *it;
        if (str::EqI(s, s2)) {
            return it.idx;
        }
    }
    return -1;
}

// returns a string
char* StrVec2::SetAt(int idx, const char* s, int sLen) {
    int idxInPage = idx;
    auto page = PageForIdx(this, idxInPage);
    if (sLen < 0) {
        sLen = str::Leni(s);
    }
    char* res = page->SetAt(idx, s, sLen);
    if (res != kNoSpace) {
        return res;
    }
    auto ss = AllocSideString(idx, s, sLen);
    ss->next = firstSide;
    firstSide = ss;
    return ss->s;
}

// remove string at idx and return it
// return value is valid as long as StrVec2 is valid
char* StrVec2::RemoveAt(int idx) {
    SideStringsRemoveAt(&firstSide, idx);
    int idxInPage = idx;
    auto page = PageForIdx(this, idxInPage);
    auto res = page->RemoveAt(idxInPage);
    UpdateSize(this);
    return res;
}

// remove string at idx more quickly but will change order of string
// return value is valid as long as StrVec2 is valid
char* StrVec2::RemoveAtFast(int idx) {
    SideStringsRemoveAt(&firstSide, idx);
    int idxInPage = idx;
    auto page = PageForIdx(this, idxInPage);
    auto res = page->RemoveAtFast(idxInPage);
    UpdateSize(this);
    return res;
}

StrVec2::iterator::iterator(const StrVec2* v, int idx) {
    this->v = v;
    this->idx = idx;
    this->idxInPage = idx;
    this->page = PageForIdx(v, this->idxInPage);
}

char* StrVec2::iterator::operator*() const {
    if (v->firstSide) {
        char* s = SideStringAt(v->firstSide, idx);
        if (s) {
            return s;
        }
    }
    return page->At(idxInPage);
}

static void Next(StrVec2::iterator& it) {
    it.idx++;
    it.idxInPage++;
    if (it.idxInPage >= it.page->nStrings) {
        it.idxInPage = 0;
        it.page = it.page->next;
    }
}

StrVec2::iterator& StrVec2::iterator::operator++(int) {
    Next(*this);
    return *this;
}

StrVec2::iterator& StrVec2::iterator::operator++() {
    Next(*this);
    return *this;
}

StrVec2::iterator& StrVec2::iterator::operator+(int n) {
    // TODO: could optimize
    for (int i = 0; i < n; i++) {
        Next(*this);
    }
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
    CompactPages(&v);

    const char* pageStart = (const char*)v.first;
    u32* b = (u32*)(pageStart + kStrVecPageHdrSize);
    u32* e = b + v.first->nStrings;
    std::sort(b, e, [pageStart, lessFn](u32 off1, u32 off2) -> bool {
        const char* s1 = (off1 == kNullIdx) ? nullptr : pageStart + off1;
        const char* s2 = (off2 == kNullIdx) ? nullptr : pageStart + off2;
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

/* splits a string into several substrings, separated by the separator
(optionally collapsing several consecutive separators into one);
e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
(resp. "a", "b", "c" if separators are collapsed) */
int Split(StrVec2& v, const char* s, const char* separator, bool collapse) {
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

static int CalcCapForJoin(StrVec2& v, const char* joint) {
    // it's ok to over-estimate
    int len = v.Size();
    size_t jointLen = str::Len(joint);
    int cap = len * (int)jointLen;
    for (int i = 0; i < len; i++) {
        char* s = v.At(i);
        cap += (int)str::Len(s);
    }
    return cap + 32; // arbitrary buffer
}

static char* JoinInner(StrVec2& v, const char* joint, str::Str& res) {
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
