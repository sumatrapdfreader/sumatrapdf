/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

typedef bool (*StrLessFunc)(Str s1, Str s2);

bool StrLess(Str s1, Str s2);
bool StrLessNoCase(Str s1, Str s2);
bool StrLessNatural(Str s1, Str s2);

ByteSlice ToByteSlice(Str s);

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
    Str AtStr(int i) const;
    void* AtDataRaw(int i) const;
    char* operator[](int) const;

    char* Append(Str s);
    char* SetAt(int idx, Str s);
    char* InsertAt(int, Str s);
    char* RemoveAt(int);
    char* RemoveAtFast(int);
    bool Remove(Str s);

    int Find(Str s, int startAt = 0) const;
    int FindI(Str s, int startAt = 0) const;
    bool Contains(Str s) const;

    struct iterator {
        const StrVec* v;
        int idx;

        // perf: cache page, idxInPage from prev iteration
        int idxInPage;
        StrVecPage* page;

        iterator(const StrVec* v, int idx);
        char* operator*() const;
        Str AsStr() const;
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
    StrVecWithData() : StrVec((int)sizeof(T)) {}

    T* AtData(int i) const {
        void* res = AtDataRaw(i);
        return (T*)(res);
    }

    int Append(Str s, const T& data) {
        StrVec::Append(s);
        int idx = Size() - 1;
        T* d = AtData(idx);
        *d = data;
        return idx;
    }

    int AppendFrom(StrVecWithData<T>* src, int srcIdx) {
        Str s = src->AtStr(srcIdx);
        T* data = src->AtData(srcIdx);
        int idx = this->Append(s, *data);
        return idx;
    }
};

int AppendIfNotExists(StrVec* v, Str s);

void Sort(StrVec* v, StrLessFunc lessFn = StrLess);
void SortIndex(StrVec* v, StrLessFunc lessFn = StrLess);
void SortNoCase(StrVec*);
void SortNatural(StrVec*);

int Split(StrVec* v, Str s, Str separator, bool collapse = false, int max = -1);
char* Join(StrVec* v, Str sep = {});
TempStr JoinTemp(StrVec* v, Str sep);

StrVecPage* StrVecPageNext(StrVecPage*);
int StrVecPageSize(StrVecPage*);
