/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

typedef bool (*StrLessFunc)(const char* s1, const char* s2);

bool StrLess(const char* s1, const char* s2);
bool StrLessNoCase(const char* s1, const char* s2);
bool StrLessNatural(const char* s1, const char* s2);

ByteSlice ToByteSlice(const char* s);

struct StrVecPage;

struct StrVec {
    StrVecPage* first = nullptr;
    int* sortIndexes = nullptr;
    int nextPageSize = 256;
    int size = 0;
    int dataSize = 0;

    StrVec() = default;
    StrVec(int dataSize);
    StrVec(const StrVec& that);
    StrVec& operator=(const StrVec& that);
    ~StrVec();

    void Reset(StrVecPage* = nullptr);

    int Size() const;
    bool IsEmpty() const;
    char* At(int i) const;
    StrSpan AtSpan(int i) const;
    void* AtDataRaw(int i) const;
    char* operator[](int) const;

    char* Append(const char*, int sLen = -1);
    char* SetAt(int idx, const char*, int sLen = -1);
    char* InsertAt(int, const char*, int sLen = -1);
    char* RemoveAt(int);
    char* RemoveAtFast(int);
    bool Remove(const char*);

    int Find(const StrSpan&, int startAt = 0) const;
    int FindI(const StrSpan&, int startAt = 0) const;
    bool Contains(const char*, int sLen = -1) const;

    struct iterator {
        const StrVec* v;
        int idx;

        // perf: cache page, idxInPage from prev iteration
        int idxInPage;
        StrVecPage* page;

        iterator(const StrVec* v, int idx);
        char* operator*() const;
        StrSpan Span() const;
        iterator& operator++();   // ++it
        iterator operator++(int); // it++
        iterator& operator+(int); // it += n
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin() const;
    iterator end() const;
};

template <typename T>
struct StrVecWithData : StrVec {
    StrVecWithData() : StrVec((int)sizeof(T)) {
    }

    T* AtData(int i) const {
        void* res = AtDataRaw(i);
        return (T*)(res);
    }

    int Append(const StrSpan& s, const T& data) {
        StrVec::Append(s.CStr(), s.Size());
        int idx = Size() - 1;
        T* d = AtData(idx);
        *d = data;
        return idx;
    }

    int Append(const char* s, const T& data) {
        StrSpan sp(s);
        int idx = this->Append(sp, data);
        return idx;
    }

    int AppendFrom(StrVecWithData<T>* src, int srcIdx) {
        StrSpan s = src->AtSpan(srcIdx);
        T* data = src->AtData(srcIdx);
        int idx = this->Append(s, *data);
        return idx;
    }
};

int AppendIfNotExists(StrVec* v, const char* s, int sLen = -1);

void Sort(StrVec* v, StrLessFunc lessFn = StrLess);
void SortIndex(StrVec* v, StrLessFunc lessFn = StrLess);
void SortNoCase(StrVec*);
void SortNatural(StrVec*);

int Split(StrVec* v, const char* s, const char* separator, bool collapse = false, int max = -1);
char* Join(StrVec* v, const char* sep = nullptr);
TempStr JoinTemp(StrVec* v, const char* sep);

StrVecPage* StrVecPageNext(StrVecPage*);
int StrVecPageSize(StrVecPage*);
