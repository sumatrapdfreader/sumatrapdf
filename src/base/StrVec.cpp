/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"
#include <algorithm>

// represents null string
constexpr u32 kNullOffset = (u32)-2;

static int StrCmp(Str s1, Str s2) {
    size_t n = std::min((size_t)s1.len, (size_t)s2.len);
    int cmp = n > 0 ? memcmp(s1.s, s2.s, n) : 0;
    if (cmp != 0) {
        return cmp;
    }
    return s1.len - s2.len;
}

static int StrCmpI(Str s1, Str s2) {
    size_t n = std::min((size_t)s1.len, (size_t)s2.len);
    int cmp = n > 0 ? _strnicmp(s1.s, s2.s, n) : 0;
    if (cmp != 0) {
        return cmp;
    }
    return s1.len - s2.len;
}

bool StrLess(Str s1, Str s2) {
    if (len(s1) == 0) {
        if (len(s2) == 0) {
            return false;
        }
        return true;
    }
    if (len(s2) == 0) {
        return false;
    }
    int n = StrCmp(s1, s2);
    return n < 0;
}

bool StrLessNoCase(Str s1, Str s2) {
    if (len(s1) == 0) {
        // null / empty string is smallest
        if (len(s2) == 0) {
            return false;
        }
        return true;
    }
    if (len(s2) == 0) {
        return false;
    }
    int n = StrCmpI(s1, s2);
    return n < 0;
}

bool StrLessNatural(Str s1, Str s2) {
    int n = str::CmpNatural(s1, s2);
    return n < 0; // TODO: verify it's < and not >
}

struct PageOpResult {
    Str s;
    bool noSpace = false;

    static PageOpResult Fail() { return {{}, true}; }
};

struct StrVecPage {
    struct StrVecPage* next;
    int pageSize;
    int nStrings;
    int dataSize;
    u8* currEnd = nullptr;
    // now follows:
    // struct { size u32; offset u32}[nStrings] }
    // ... free space
    // strings (allocated from the end)

    Str AtStr(int i) const;
    void* AtDataRaw(int) const;

    Str RemoveAt(int);
    Str RemoveAtFast(int);

    int BytesLeft();
    PageOpResult Append(Str s);
    PageOpResult SetAt(int idxSet, Str s);
    PageOpResult InsertAt(int idxSet, Str s);
};

constexpr int kStrVecPageHdrSize = (int)sizeof(StrVecPage);

static int cbOffsetsSize(int nStrings, int dataSize) {
    ReportIf(dataSize % 4 != 0);
    int nOffsets = (2 * (int)sizeof(u32)) + dataSize;
    return nStrings * nOffsets;
}

int StrVecPage::BytesLeft() {
    u8* start = (u8*)this;
    start += kStrVecPageHdrSize;
    int cbTotal = (int)(currEnd - start);
    int cbOffsets = cbOffsetsSize(nStrings, dataSize);
    auto res = cbTotal - cbOffsets;
    ReportIf(res < 0);
    return res;
}

static StrVecPage* AllocStrVecPage(int pageSize, int dataSize) {
    auto page = (StrVecPage*)AllocZero(nullptr, pageSize);
    page->next = nullptr;
    page->nStrings = 0;
    page->pageSize = pageSize;
    page->dataSize = dataSize;
    u8* start = (u8*)page;
    page->currEnd = start + pageSize;
    return page;
}

// how many bytes per index entry with data
// index entry is offset and size (both u32) + (optional) data
static int cbIndexSize(int dataSize) {
    // dataSize is guaranteed multiple of sizeof(u32)
    return (2 * sizeof(u32)) + dataSize;
}

static u32* OffsetsForString(const StrVecPage* p, int idx) {
    ReportIf(idx < 0 || idx > p->nStrings);
    u8* off = (u8*)p;
    off += kStrVecPageHdrSize;
    off += idx * cbIndexSize(p->dataSize);
    return (u32*)off;
}

// must have enough space
static Str AppendJustString(StrVecPage* p, Str s, int idx) {
    ReportIf(str::IsNull(s));
    int sLen = s.len;
    u32* offsets = OffsetsForString(p, idx);
    u8* dst = p->currEnd - sLen - 1; // 1 for zero termination
    u32 off = (u32)(dst - (u8*)p);
    offsets[0] = (u32)off;
    offsets[1] = (u32)sLen;
    memcpy(dst, s.s, (size_t)sLen);
    dst[sLen] = 0; // zero-terminate for C compat
    p->currEnd = dst;
    return Str((char*)dst, sLen);
}

PageOpResult StrVecPage::SetAt(int idx, Str s) {
    u32* offsets = OffsetsForString(this, idx);
    if (str::IsNull(s)) {
        // fast path for null, doesn't require new space at all
        offsets[0] = kNullOffset;
        offsets[1] = 0;
        return {};
    }
    int sLen = s.len;
    u32 off = offsets[0];
    u8* start = (u8*)this;
    if (off != kNullOffset) {
        // fast path for when new string is smaller than the current string
        int currLen = (int)offsets[1];
        if (sLen <= currLen) {
            auto dst = start + off;
            memcpy(dst, s.s, (size_t)sLen);
            dst[sLen] = 0; // zero-terminate for C compat
            offsets[1] = (u32)sLen;
            return {Str((char*)dst, sLen), false};
        }
    }

    int cbNeeded = sLen + 1; // +1 for zero termination

    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return PageOpResult::Fail();
    }
    return {AppendJustString(this, s, idx), false};
}

PageOpResult StrVecPage::InsertAt(int idx, Str s) {
    ReportIf(idx < 0 || idx > nStrings);

    int cbIndex = cbIndexSize(dataSize);
    int cbNeeded = cbIndex;
    if (!str::IsNull(s)) {
        cbNeeded += s.len + 1; // +1 for zero termination
    }
    int cbLeft = BytesLeft();
    if (cbNeeded > cbLeft) {
        return PageOpResult::Fail();
    }

    u32* offsets = OffsetsForString(this, idx);
    if (idx != nStrings) {
        // make space for idx
        u32* src = offsets;
        u32* dst = OffsetsForString(this, idx + 1);
        int nToCopy = (nStrings - idx) * cbIndex;
        memmove(dst, src, (size_t)nToCopy);
    }

    PageOpResult res;
    if (!str::IsNull(s)) {
        res = {AppendJustString(this, s, idx), false};
    } else {
        offsets[0] = kNullOffset;
        offsets[1] = 0;
    }
    nStrings++;
    return res;
}

PageOpResult StrVecPage::Append(Str s) {
    return InsertAt(nStrings, s);
}

Str StrVecPage::AtStr(int idx) const {
    ReportIf(idx >= nStrings);
    u8* start = (u8*)this;
    u32* offsets = OffsetsForString(this, idx);
    u32 off = offsets[0];
    int sLen = (int)offsets[1];
    if (off == kNullOffset) {
        ReportIf(sLen != 0);
        return {};
    }
    return Str((char*)(start + off), sLen);
}

void* StrVecPage::AtDataRaw(int idx) const {
    u32* offsets = OffsetsForString(this, idx) + 2;
    return (void*)(offsets);
}

// we don't de-allocate removed strings so we can safely return the string
Str StrVecPage::RemoveAt(int idx) {
    ReportIf(nStrings <= 0 || idx >= nStrings);
    Str removed = AtStr(idx);
    nStrings--;
    int nToCopy = cbOffsetsSize(nStrings - idx, dataSize);
    if (nToCopy == 0) {
        // last string
        return removed;
    }
    u32* dst = OffsetsForString(this, idx);
    u32* src = OffsetsForString(this, idx + 1);
    memmove((void*)dst, (void*)src, nToCopy);
    return removed;
}

// we don't de-allocate removed strings so we can safely return the string
Str StrVecPage::RemoveAtFast(int idx) {
    ReportIf(nStrings <= 0 || idx >= nStrings);
    Str removed = AtStr(idx);
    nStrings--;
    if (idx == nStrings) {
        // last string
        return removed;
    }
    // over-write idx with last string idx
    u32* dst = OffsetsForString(this, idx);
    u32* src = OffsetsForString(this, nStrings);
    int nToCopy = cbIndexSize(dataSize);
    memmove((void*)dst, (void*)src, nToCopy);
    return removed;
}

static void FreePages(StrVecPage* toFree) {
    StrVecPage* next;
    while (toFree) {
        next = toFree->next;
        Free(nullptr, toFree);
        toFree = next;
    }
}

static StrVecPage* CompactStrVecPages(StrVecPage* first, int extraSize) {
    if (!first) {
        ReportIf(extraSize > 0);
        return nullptr;
    }
    int dataSize = first->dataSize;
    auto curr = first;
    u8 *pageStart, *pageEnd;
    int cbStrings;
    int nStrings = 0;
    int cbStringsTotal = 0; // including 0-termination
    while (curr) {
        nStrings += curr->nStrings;
        pageStart = (u8*)curr;
        pageEnd = pageStart + curr->pageSize;
        cbStrings = (int)(pageEnd - curr->currEnd);
        cbStringsTotal += cbStrings;
        curr = curr->next;
    }
    // strLenTotal might be more than needed if we removed strings, but that's ok
    int pageSize = kStrVecPageHdrSize + cbOffsetsSize(nStrings, dataSize) + cbStringsTotal;
    if (extraSize > 0) {
        pageSize += extraSize;
    }
    pageSize = RoundUp(pageSize, 64); // jic
    auto page = AllocStrVecPage(pageSize, dataSize);
    int n;
    Str s;
    curr = first;
    int nStr = 0;
    while (curr) {
        n = curr->nStrings;
        // TODO(perf): could optimize slightly
        for (int i = 0; i < n; i++) {
            s = curr->AtStr(i);
            page->Append(s);
            if (dataSize > 0) {
                void* dst = page->AtDataRaw(nStr);
                void* src = curr->AtDataRaw(i);
                memcpy(dst, src, dataSize);
                nStr++;
            }
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

static inline void InvalidateSortIndexes(StrVec* v) {
    if (v->sortIndexes) {
        Free(nullptr, v->sortIndexes);
        v->sortIndexes = nullptr;
    }
}

void StrVec::Reset(StrVecPage* initWith) {
    InvalidateSortIndexes(this);
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

StrVec::StrVec(int dataSize) {
    if (dataSize == 0) {
        return;
    }
    this->dataSize = RoundUp(dataSize, (int)sizeof(u32));
}

StrVec::~StrVec() {
    Reset(nullptr);
}

StrVec::StrVec(const StrVec& that) {
    Reset(that.first);
    dataSize = that.dataSize;
}

StrVec& StrVec::operator=(const StrVec& that) {
    if (this == &that) {
        return *this;
    }
    Reset(that.first);
    dataSize = that.dataSize;
    return *this;
}

bool StrVec::IsEmpty() const {
    return size == 0;
}

StrVecPage* StrVecPageNext(StrVecPage* page) {
    return page->next;
}

int StrVecPageSize(StrVecPage* page) {
    return page->nStrings;
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
    auto page = AllocStrVecPage(pageSize, v->dataSize);
    if (last) {
        ReportIf(!v->first);
        last->next = page;
    } else {
        ReportIf(v->first);
        v->first = page;
    }
    return page;
}

Str StrVec::Append(Str s) {
    int cbIndex = cbIndexSize(dataSize);
    int cbNeeded = cbIndex;
    if (!str::IsNull(s)) {
        cbNeeded += (s.len + 1); // +1 for zero termination
    }
    auto last = first;
    while (last && last->next) {
        last = last->next;
    }
    if (!last || last->BytesLeft() < cbNeeded) {
        last = AllocatePage(this, last, cbNeeded);
    }
    auto res = last->Append(s);
    ReportIf(res.noSpace);
    size++;
    InvalidateSortIndexes(this);
    return res.s;
}

// returns index of inserted string, -1 if not inserted
int AppendIfNotExists(StrVec* v, Str s) {
    if (v->Contains(s)) {
        return -1;
    }
    int idx = len(*v);
    v->Append(s);
    return idx;
}

static StrVecPage* PageForIdx(const StrVec* v, int idx, int* idxInPageOut) {
    auto page = v->first;
    while (page) {
        if (page->nStrings > idx) {
            *idxInPageOut = idx;
            return page;
        }
        idx -= page->nStrings;
        page = page->next;
    }
    *idxInPageOut = 0;
    return page;
}

// returns a string
// note: this might invalidate previously returned strings because
// it might re-allocate memory used for those strings
Str StrVec::SetAt(int idx, Str s) {
    {
        int idxInPage;
        auto page = PageForIdx(this, idx, &idxInPage);
        auto res = page->SetAt(idxInPage, s);
        if (!res.noSpace) {
            InvalidateSortIndexes(this);
            return res.s;
        }
    }
    // perf: we assume that there will be more SetAt() calls so pre-allocate
    // extra space to make many SetAt() calls less expensive
    int extraSpace = RoundUp(s.len + 1, 2048);
    CompactPages(this, extraSpace);
    auto res = first->SetAt(idx, s);
    ReportIf(res.noSpace);
    InvalidateSortIndexes(this);
    return res.s;
}

// returns a string
// note: this might invalidate previously returned strings because
// it might re-allocate memory used for those strings
Str StrVec::InsertAt(int idx, Str s) {
    if (idx == size) {
        return Append(s);
    }

    {
        int idxInPage;
        auto page = PageForIdx(this, idx, &idxInPage);
        auto res = page->InsertAt(idxInPage, s);
        if (!res.noSpace) {
            size++;
            InvalidateSortIndexes(this);
            return res.s;
        }
    }

    // perf: we assume that there will be more InsertAt() calls so pre-allocate
    // extra space to make many InsertAt() calls less expensive
    int extraSpace = RoundUp(s.len + 1, 2048);
    CompactPages(this, extraSpace);
    auto res = first->InsertAt(idx, s);
    ReportIf(res.noSpace);
    size++;
    InvalidateSortIndexes(this);
    return res.s;
}

// remove string at idx and return it
// return value is valid as long as StrVec is valid
Str StrVec::RemoveAt(int idx) {
    int idxInPage;
    auto page = PageForIdx(this, idx, &idxInPage);
    Str removed = page->AtStr(idxInPage);
    page->RemoveAt(idxInPage);
    size--;
    InvalidateSortIndexes(this);
    return removed;
}

// remove string at idx more quickly but will change order of string
// return value is valid as long as StrVec is valid
Str StrVec::RemoveAtFast(int idx) {
    int idxInPage;
    auto page = PageForIdx(this, idx, &idxInPage);
    Str removed = page->AtStr(idxInPage);
    page->RemoveAtFast(idxInPage);
    size--;
    InvalidateSortIndexes(this);
    return removed;
}

// return true if did remove
bool StrVec::Remove(Str s) {
    int idx = Find(s);
    if (idx >= 0) {
        RemoveAt(idx);
        return true;
    }
    return false;
}

Str StrVec::At(int idx) const {
    if (sortIndexes) {
        idx = sortIndexes[idx];
    }
    int idxInPage;
    auto page = PageForIdx(this, idx, &idxInPage);
    return page->AtStr(idxInPage);
}

void* StrVec::AtDataRaw(int idx) const {
    ReportIf(dataSize == 0); // shouldn't call
    if (sortIndexes) {
        idx = sortIndexes[idx];
    }
    int idxInPage;
    auto page = PageForIdx(this, idx, &idxInPage);
    return page->AtDataRaw(idxInPage);
}

Str StrVec::operator[](int idx) const {
    ReportIf(idx < 0);
    return At(idx);
}

int StrVec::Find(Str s, int startAt) const {
    int sLen = s.len;
    auto end = this->end();
    for (auto it = this->begin() + startAt; it != end; it++) {
        Str s2 = *it;
        if (s2.len == sLen && str::Eq(s, s2)) {
            return it.idx;
        }
    }
    return -1;
}

int StrVec::FindI(Str s, int startAt) const {
    int sLen = s.len;
    auto end = this->end();
    for (auto it = this->begin() + startAt; it != end; it++) {
        Str s2 = *it;
        if (s2.len == sLen && str::EqI(s, s2)) {
            return it.idx;
        }
    }
    return -1;
}

bool StrVec::Contains(Str s) const {
    int idx = Find(s);
    return idx != -1;
}

StrVec::iterator::iterator(const StrVec* v, int idx) {
    this->v = v;
    this->idx = idx;
    if (this->v->sortIndexes) {
        return;
    }
    int idxInPage;
    auto page = PageForIdx(v, idx, &idxInPage);
    this->page = page;
    this->idxInPage = idxInPage;
}

StrVec::iterator StrVec::begin() const {
    return StrVec::iterator(this, 0);
}

StrVec::iterator StrVec::end() const {
    return StrVec::iterator(this, len(*this));
}

Str StrVec::iterator::operator*() const {
    if (this->v->sortIndexes) {
        return v->At(idx);
    }
    return page->AtStr(idxInPage);
}

static void AdvanceStrVecIter(StrVec::iterator& it, int n) {
    if (it.v->sortIndexes) {
        it.idx += n;
        return;
    }
    // TODO: optimize for n > 1
    for (int i = 0; i < n; i++) {
        it.idx++;
        it.idxInPage++;
        if (it.idxInPage >= it.page->nStrings) {
            it.idxInPage = 0;
            it.page = it.page->next;
            while (it.page && it.page->nStrings == 0) {
                it.page = it.page->next;
            }
        }
    }
}

// postfix increment
StrVec::iterator StrVec::iterator::operator++(int) {
    auto res = *this;
    AdvanceStrVecIter(*this, 1);
    return res;
}

StrVec::iterator& StrVec::iterator::operator++() {
    AdvanceStrVecIter(*this, 1);
    return *this;
}

StrVec::iterator& StrVec::iterator::operator+(int n) {
    AdvanceStrVecIter(*this, n);
    return *this;
}

bool operator==(const StrVec::iterator& a, const StrVec::iterator& b) {
    return (a.v == b.v) && (a.idx == b.idx);
};

bool operator!=(const StrVec::iterator& a, const StrVec::iterator& b) {
    return (a.v != b.v) || (a.idx != b.idx);
};

static void SortNoData(StrVec* v, StrLessFunc lessFn) {
    CompactPages(v, 0);
    if (len(*v) < 2) {
        return;
    }
    ReportIf(!v->first);
    ReportIf(v->first->next);
    int n = len(*v);

    u8* pageStart = (u8*)v->first;
    u64* b = (u64*)(pageStart + kStrVecPageHdrSize);
    u64* e = b + n;
    std::sort(b, e, [pageStart, lessFn](u64 offLen1, u64 offLen2) -> bool {
        u32 off1 = (u32)(offLen1 & 0xffffffff);
        u32 off2 = (u32)(offLen2 & 0xffffffff);
        Str s1 = (off1 == kNullOffset) ? Str{} : Str((char*)(pageStart + off1));
        Str s2 = (off2 == kNullOffset) ? Str{} : Str((char*)(pageStart + off2));
        bool ret = lessFn(s1, s2);
        return ret;
    });
}

static int* AllocateSortIndexes(StrVec* v) {
    InvalidateSortIndexes(v);
    int n = len(*v);
    auto res = AllocArray<int>(n);
    for (int i = 0; i < n; i++) {
        res[i] = i;
    }
    return res;
}

void SortIndex(StrVec* v, StrLessFunc lessFn) {
    if (len(*v) < 2) {
        return;
    }
    int* indexes = AllocateSortIndexes(v);
    int n = len(*v);
    int* b = indexes;
    int* e = indexes + n;
    std::sort(b, e, [v, lessFn](int idx1, int idx2) -> bool {
        Str s1 = v->At(idx1);
        Str s2 = v->At(idx2);
        bool ret = lessFn(s1, s2);
        return ret;
    });
    v->sortIndexes = indexes;
}

void Sort(StrVec* v, StrLessFunc lessFn) {
    if (len(*v) < 2) {
        return;
    }
    if (v->dataSize == 0) {
        SortNoData(v, lessFn);
        return;
    }
    SortIndex(v, lessFn);
}

void SortNoCase(StrVec* v) {
    Sort(v, StrLessNoCase);
}

void SortNatural(StrVec* v) {
    Sort(v, StrLessNatural);
}

static bool reachedMax(int nAdded, int max) {
    if (max < 0) {
        return false;
    }
    if (max == 0) {
        max = 1;
    }
    return nAdded >= max;
}

/* splits a string into several substrings, separated by the separator
    (optionally collapsing several consecutive separators into one);
    e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
    (resp. "a", "b", "c" if separators are collapsed) */
int Split(StrVec* v, Str s, Str separator, bool collapse, int max) {
    int off = 0;
    int nAdded = 0;
    while (true) {
        if (reachedMax(nAdded, max)) {
            return nAdded;
        }
        Str rest = Str(s.s + off, s.len - off);
        int idx = str::IndexOf(rest, separator);
        if (idx < 0) {
            break;
        }
        if (!collapse || idx > 0) {
            nAdded++;
            if (reachedMax(nAdded, max)) {
                // this is the last one
                v->Append(rest);
                return nAdded;
            }
            v->Append(Str(rest.s, idx));
        }
        off += idx + separator.len;
    }
    bool shouldAddRest = true;
    if (off >= s.len) {
        // if we're collapsing, we're not adding empty string
        // at the end, unless we haven't added any strings yet
        // i.e. to match other languages, "".split(" ") => [""]
        shouldAddRest = !collapse || len(*v) == 0;
    }
    if (shouldAddRest) {
        v->Append(Str(s.s + off, s.len - off));
        nAdded++;
    }
    return nAdded;
}

static int CalcCapForJoin(const StrVec* v, Str joint) {
    // it's ok to over-estimate
    int cap = 0;
    int jointLen = joint.len;
    for (auto it = v->begin(); it != v->end(); it++) {
        Str s = *it;
        cap += s.len + 1 + jointLen;
    }
    return cap + 32; // +32 arbitrary buffer
}

static void JoinInner(const StrVec* v, Str joint, str::Builder& res) {
    int jointLen = joint.len;
    // TODO: possibly not handling null values in the middle. need to add more tests and fix
    int firstForJoint = 0;
    int i = 0;
    for (auto it = v->begin(); it != v->end(); it++) {
        Str s = *it;
        if (str::IsNull(s)) {
            firstForJoint++;
            i++;
            continue;
        }
        if (i > firstForJoint && jointLen > 0) {
            res.Append(joint);
        }
        res.Append(s);
        i++;
    }
}

Str Join(StrVec* v, Str joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Builder tmp(capHint);
    JoinInner(v, joint, tmp);
    return tmp.TakeStr();
}

TempStr JoinTemp(StrVec* v, Str joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Builder tmp(capHint, GetTempArena());
    JoinInner(v, joint, tmp);
    return ToStrTemp(tmp);
}
