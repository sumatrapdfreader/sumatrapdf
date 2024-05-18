/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

typedef bool (*StrLessFunc)(const char* s1, const char* s2);

// strings are stored linearly in strings, separated by 0
// index is an array of indexes i.e. strings[index[2]] is
// beginning of string at index 2
struct StrVec {
    str::Str strings;
    Vec<u32> index;

    StrVec() = default;
    ~StrVec() = default;
    void Reset();

    int Size() const;
    char* At(int) const;
    char* operator[](int) const;

    int Append(const char*, int len = -1);
    int AppendIfNotExists(const char*);
    bool InsertAt(int, const char*);
    char* SetAt(int idx, const char* s);
    int Find(const char*, int startAt = 0) const;
    int FindI(const char*, int startAt = 0) const;
    bool Contains(const char*) const;
    char* RemoveAtFast(int idx);
    char* RemoveAt(int idx);
    bool Remove(const char*);

    struct Iterator {
        StrVec* v;
        int idx;

        Iterator(StrVec* v, int i) : v(v), idx(i) {
        }
        char* operator*() const;
        Iterator& operator++();
        Iterator operator++(int);
        friend bool operator==(const Iterator& a, const Iterator& b);
        friend bool operator!=(const Iterator& a, const Iterator& b);
    };
    Iterator begin() {
        return Iterator(this, 0);
    }
    Iterator end() {
        return Iterator(this, this->Size());
    }
};

void Sort(StrVec& v, StrLessFunc lessFn = nullptr);
void SortNoCase(StrVec&);
void SortNatural(StrVec&);

int Split(StrVec& v, const char* s, const char* separator, bool collapse = false);
char* Join(const StrVec& v, const char* sep = nullptr);
TempStr JoinTemp(const StrVec& v, const char* sep);
ByteSlice ToByteSlice(const char* s);

struct StrVecPage;
struct SideString;

struct StrVec2 {
    StrVecPage* first = nullptr;
    StrVecPage* curr = nullptr;
    SideString* firstSide = nullptr;
    int nextPageSize = 256;
    int cachedSize = 0;

    StrVec2() = default;
    StrVec2(const StrVec2& that);
    StrVec2& operator=(const StrVec2& that);
    ~StrVec2();

    void Reset();

    int Size() const;
    char* At(int i) const;
    char* operator[](int) const;

    char* Append(const char* s, int sLen = -1);
    char* RemoveAt(int);
    char* RemoveAtFast(int);
    char* SetAt(int idx, const char* s, int sLen = -1);

    int Find(const char* sv, int startAt = 0) const;
    int FindI(const char* sv, int startAt = 0) const;

    struct iterator {
        // TODO: could optimize
        StrVec2* v = nullptr;
        int idx = 0;

        iterator() = default;
        iterator(StrVec2* v, int idx) : v(v), idx(idx) {
        }

        char* operator*() const;
        iterator& operator++();
        iterator operator++(int);
        friend bool operator==(const iterator& a, const iterator& b);
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin() {
        return iterator{this, 0};
    }
    iterator end() {
        int n = this->Size();
        return iterator{this, n};
    }
};

void Sort(StrVec2& v, StrLessFunc lessFn = nullptr);
void SortNoCase(StrVec2&);
void SortNatural(StrVec2&);

int Split(StrVec2& v, const char* s, const char* separator, bool collapse = false);
char* Join(StrVec2& v, const char* sep = nullptr);
TempStr JoinTemp(StrVec2& v, const char* sep);
