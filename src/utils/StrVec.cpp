/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

ByteSlice ToByteSlice(const char* s) {
    size_t n = str::Len(s);
    return {(u8*)s, n};
}

// represents null string
constexpr u32 kNullOffset = (u32)-2;

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
    int cap = 0;
    int jointLen = str::Leni(joint);
    for (auto it = v.begin(); it != v.end(); it++) {
        auto s = it.Span();
        cap += s.Size() + 1 + jointLen;
    }
    return cap + 32; // +32 arbitrary buffer
}

static char* JoinInner(const StrVec& v, const char* joint, str::Str& res) {
    int len = v.Size();
    int jointLen = str::Leni(joint);
    // TODO: possibly not handling null values in the middle. need to add more tests and fix
    int firstForJoint = 0;
    int i = 0;
    for (auto it = v.begin(); it != v.end(); it++) {
        auto s = it.Span();
        if (!s.CStr()) {
            firstForJoint++;
            i++;
            continue;
        }
        if (i > firstForJoint && jointLen > 0) {
            res.Append(joint, jointLen);
        }
        res.Append(s.CStr(), s.Len());
        i++;
    }
    return res.StealData();
}

struct StrVecPage {
    struct StrVecPage* next;
    int pageSize;
    int nStrings;
    char* currEnd;
    // now follows:
    // struct { size u32; offset u32}[nStrings] }
    // ... free space
    // strings (allocated from the end)

    char* At(int) const;
    StrSpan AtSpan(int i) const;
    char* RemoveAt(int);
    char* RemoveAtFast(int);

    char* AtHelper(int, int& sLen) const;
    int BytesLeft();
    char* Append(const char* s, int sLen);
    char* Append(const StrSpan&);
    char* SetAt(int idxSet, const char* s, int sLen);
    char* InsertAt(int idxSet, const char* s, int sLen);
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
    ReportIf(res < 0);
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
    ReportIf(idx < 0 || idx > p->nStrings);
    char* start = (char*)p;
    u32* offsets = (u32*)(start + kStrVecPageHdrSize);
    return offsets + (idx * 2);
}

// must have enough space
static char* AppendJustString(StrVecPage* p, const char* s, int sLen, int idx) {
    ReportIf(!s);
    u32* offsets = OffsetsForString(p, idx);
    char* dst = p->currEnd - sLen - 1; // 1 for zero termination
    u32 off = (u32)(dst - (char*)p);
    offsets[0] = (u32)off;
    offsets[1] = (u32)sLen;
    memcpy(dst, s, (size_t)sLen);
    dst[sLen] = 0; // zero-terminate for C compat
    p->currEnd = dst;
    return dst;
}

char* StrVecPage::SetAt(int idx, const char* s, int sLen) {
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
            auto dst = start + off;
            memcpy(dst, s, (size_t)sLen);
            dst[sLen] = 0; // zero-terminate for C compat
            return dst;
        }
    }

    int cbNeeded = 0; // when replacing, we re-use offset / size slots
    if (s) {
        cbNeeded += sLen + 1; // +1 for zero termination
    }

    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return kNoSpace;
    }
    return AppendJustString(this, s, sLen, idx);
}

char* StrVecPage::InsertAt(int idx, const char* s, int sLen) {
    ReportIf(idx < 0 || idx > nStrings);

    int cbNeeded = sizeof(u32) * 2; // for offset / size
    if (s) {
        cbNeeded += sLen + 1; // +1 for zero termination
    }
    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return kNoSpace;
    }

    u32* offsets = OffsetsForString(this, idx);
    if (idx != nStrings) {
        // make space for idx
        u32* src = offsets;
        u32* dst = OffsetsForString(this, idx + 1);
        size_t nToCopy = (nStrings - idx) * 2 * sizeof(u32);
        memmove(dst, src, nToCopy);
    }

    if (s) {
        s = AppendJustString(this, s, sLen, idx);
    } else {
        offsets[0] = kNullOffset;
        offsets[1] = 0;
        s = nullptr;
    }
    nStrings++;
    return (char*)s;
}
char* StrVecPage::Append(const StrSpan& s) {
    return InsertAt(nStrings, s.CStr(), s.Len());
}

char* StrVecPage::Append(const char* s, int sLen) {
    return InsertAt(nStrings, s, sLen);
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
        ReportIf(extraSize > 0);
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
            page->Append(s);
        }
        curr = curr->next;
    }
    return page;
}

static void CompactPages(StrVec* v, int extraSize) {
    auto first = CompactStrVecPages(v->first, extraSize);
    FreePages(v->first);
    v->first = first;
    ReportIf(first && (v->size != first->nStrings));
}

void StrVec::Reset(StrVecPage* initWith) {
    FreePages(first);
    first = nullptr;
    nextPageSize = 256; // TODO: or leave it alone?
    size = 0;
    if (initWith == nullptr) {
        return;
    }
    first = CompactStrVecPages(initWith, 0);
    size = first->nStrings;
}

StrVec::~StrVec() {
    Reset(nullptr);
}

StrVec::StrVec(const StrVec& that) {
    Reset(that.first);
}

StrVec& StrVec::operator=(const StrVec& that) {
    if (this == &that) {
        return *this;
    }
    Reset(that.first);
    return *this;
}

int StrVec::Size() const {
    return size;
}

bool StrVec::IsEmpty() const {
    return size == 0;
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

static StrVecPage* AllocatePage(StrVec* v, StrVecPage* last, int nBytesNeeded) {
    int minPageSize = kStrVecPageHdrSize + nBytesNeeded;
    int pageSize = RoundUp(minPageSize, 8);
    if (pageSize < v->nextPageSize) {
        pageSize = v->nextPageSize;
        v->nextPageSize = CalcNextPageSize(v->nextPageSize);
    }
    auto page = AllocStrVecPage(pageSize);
    if (last) {
        ReportIf(!v->first);
        last->next = page;
    } else {
        ReportIf(v->first);
        v->first = page;
    }
    return page;
}

char* StrVec::Append(const char* s, int sLen) {
    if (sLen < 0) {
        sLen = str::Leni(s);
    }
    int nBytesNeeded = sizeof(u32) * 2; // for index and size
    if (s) {
        nBytesNeeded += (sLen + 1); // +1 for zero termination
    }
    auto last = first;
    while (last && last->next) {
        last = last->next;
    }
    if (!last || last->BytesLeft() < nBytesNeeded) {
        last = AllocatePage(this, last, nBytesNeeded);
    }
    auto res = last->Append(s, sLen);
    size++;
    return res;
}

// returns index of inserted string, -1 if not inserted
int AppendIfNotExists(StrVec& v, const char* s, int sLen) {
    if (sLen < 0) {
        sLen = str::Leni(s);
    }
    if (v.Contains(s, sLen)) {
        return -1;
    }
    int idx = v.Size();
    v.Append(s, sLen);
    return idx;
}

static std::pair<StrVecPage*, int> PageForIdx(const StrVec* v, int idx) {
    auto page = v->first;
    while (page && idx > 0) {
        if (page->nStrings > idx) {
            return {page, idx};
        }
        idx -= page->nStrings;
        page = page->next;
    }
    return {page, 0};
}

// returns a string
// note: this might invalidate previously returned strings because
// it might re-allocate memore used for those strings
char* StrVec::SetAt(int idx, const char* s, int sLen) {
    {
        auto [page, idxInPage] = PageForIdx(this, idx);
        if (sLen < 0) {
            sLen = str::Leni(s);
        }
        char* res = page->SetAt(idxInPage, s, sLen);
        if (res != kNoSpace) {
            return res;
        }
    }
    // perf: we assume that there will be more SetAt() calls so pre-allocate
    // extra space to make many SetAt() calls less expensive
    int extraSpace = RoundUp(sLen + 1, 2048);
    CompactPages(this, extraSpace);
    char* res = first->SetAt(idx, s, sLen);
    ReportIf(res == kNoSpace);
    return res;
}

// returns a string
// note: this might invalidate previously returned strings because
// it might re-allocate memore used for those strings
char* StrVec::InsertAt(int idx, const char* s, int sLen) {
    if (idx == size) {
        return Append(s, sLen);
    }
    {
        auto [page, idxInPage] = PageForIdx(this, idx);
        if (sLen < 0) {
            sLen = str::Leni(s);
        }
        char* res = page->InsertAt(idxInPage, s, sLen);
        if (res != kNoSpace) {
            return res;
        }
    }
    // perf: we assume that there will be more SetAt() calls so pre-allocate
    // extra space to make many SetAt() calls less expensive
    int extraSpace = RoundUp(sLen + 1, 2048);
    CompactPages(this, extraSpace);
    char* res = first->InsertAt(idx, s, sLen);
    ReportIf(res == kNoSpace);
    return res;
}

// remove string at idx and return it
// return value is valid as long as StrVec is valid
char* StrVec::RemoveAt(int idx) {
    auto [page, idxInPage] = PageForIdx(this, idx);
    auto res = page->RemoveAt(idxInPage);
    size--;
    return res;
}

// remove string at idx more quickly but will change order of string
// return value is valid as long as StrVec is valid
char* StrVec::RemoveAtFast(int idx) {
    auto [page, idxInPage] = PageForIdx(this, idx);
    auto res = page->RemoveAtFast(idxInPage);
    size--;
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

char* StrVec::At(int idx) const {
    auto [page, idxInPage] = PageForIdx(this, idx);
    return page->At(idxInPage);
}

StrSpan StrVec::AtSpan(int idx) const {
    auto [page, idxInPage] = PageForIdx(this, idx);
    return page->AtSpan(idxInPage);
}

char* StrVec::operator[](int idx) const {
    ReportIf(idx < 0);
    return At(idx);
}

int StrVec::Find(const char* s, int startAt) const {
    int sLen = str::Leni(s);
    auto end = this->end();
    for (auto it = this->begin() + startAt; it != end; it++) {
        StrSpan s2 = it.Span();
        if (s2.Len() == sLen && str::Eq(s, s2.CStr())) {
            return it.idx;
        }
    }
    return -1;
}

int StrVec::FindI(const char* s, int startAt) const {
    int sLen = str::Leni(s);
    auto end = this->end();
    for (auto it = this->begin() + startAt; it != end; it++) {
        StrSpan s2 = it.Span();
        if (s2.Len() == sLen && str::EqI(s, s2.CStr())) {
            return it.idx;
        }
    }
    return -1;
}

// TODO: needs to use sLen
bool StrVec::Contains(const char* s, int) const {
    int idx = Find(s);
    return idx != -1;
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
    auto [page, idxInPage] = PageForIdx(v, idx);
    this->page = page;
    this->idxInPage = idxInPage;
}

char* StrVec::iterator::operator*() const {
    return page->At(idxInPage);
}

StrSpan StrVec::iterator::Span() const {
    return page->AtSpan(idxInPage);
}

static void Next(StrVec::iterator& it, int n) {
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

StrVec::iterator& StrVec::iterator::operator++(int) {
    Next(*this, 1);
    return *this;
}

StrVec::iterator& StrVec::iterator::operator++() {
    Next(*this, 1);
    return *this;
}

StrVec::iterator& StrVec::iterator::operator+(int n) {
    Next(*this, n);
    return *this;
}

bool operator==(const StrVec::iterator& a, const StrVec::iterator& b) {
    return (a.v == b.v) && (a.idx == b.idx);
};

bool operator!=(const StrVec::iterator& a, const StrVec::iterator& b) {
    return (a.v != b.v) || (a.idx != b.idx);
};

void Sort(StrVec& v, StrLessFunc lessFn) {
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
    ReportIf(!v.first);
    ReportIf(v.first->next);
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

void SortNoCase(StrVec& v) {
    Sort(v, strLessNoCase);
}

void SortNatural(StrVec& v) {
    Sort(v, strLessNatural);
}

char* Join(StrVec& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint);
    return JoinInner(v, joint, tmp);
}

TempStr JoinTemp(StrVec& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint, GetTempAllocator());
    return JoinInner(v, joint, tmp);
}
