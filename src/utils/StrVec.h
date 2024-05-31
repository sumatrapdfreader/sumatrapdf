/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

typedef bool (*StrLessFunc)(const char* s1, const char* s2);

ByteSlice ToByteSlice(const char* s);

struct StrVecPage;

struct StrVec {
    StrVecPage* first = nullptr;
    int nextPageSize = 256;
    int size = 0;

    StrVec() = default;
    StrVec(const StrVec& that);
    StrVec& operator=(const StrVec& that);
    ~StrVec();

    void Reset(StrVecPage* = nullptr);

    int Size() const;
    bool IsEmpty() const;
    char* At(int i) const;
    StrSpan AtSpan(int i) const;
    char* operator[](int) const;

    char* Append(const char*, int sLen = -1);
    char* SetAt(int idx, const char*, int sLen = -1);
    char* InsertAt(int, const char*, int sLen = -1);
    char* RemoveAt(int);
    char* RemoveAtFast(int);
    bool Remove(const char*);

    int Find(const char*, int startAt = 0) const;
    int FindI(const char*, int startAt = 0) const;
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
        iterator& operator++();    // ++it
        iterator& operator++(int); // it++
        iterator& operator+(int);  // it += n
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin() const;
    iterator end() const;
};

int AppendIfNotExists(StrVec& v, const char* s, int sLen = -1);

void Sort(StrVec& v, StrLessFunc lessFn = nullptr);
void SortNoCase(StrVec&);
void SortNatural(StrVec&);

int Split(StrVec& v, const char* s, const char* separator, bool collapse = false);
char* Join(StrVec& v, const char* sep = nullptr);
TempStr JoinTemp(StrVec& v, const char* sep);
