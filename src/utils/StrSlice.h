/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct ByteSlice {
    u8* d = nullptr;
    size_t sz = 0;

    ByteSlice() = default;
    ~ByteSlice() = default;
    ByteSlice(const char* str) {
        d = (u8*)str;
        sz = strlen(str);
    }
    ByteSlice(char* str) {
        d = (u8*)str;
        sz = strlen(str);
    }
    ByteSlice(u8* data, size_t size) {
        d = data;
        sz = size;
    }
    ByteSlice(const ByteSlice& data) {
        d = data.data();
        sz = data.size();
    }
    ByteSlice& operator=(const ByteSlice& other) {
        d = other.d;
        sz = other.sz;
        return *this;
    }
    u8* data() const {
        return d;
    }
    size_t size() const {
        return sz;
    }
    bool empty() const {
        return !d || sz == 0;
    }
    ByteSlice Clone() const {
        if (empty()) {
            return {};
        }
        u8* res = (u8*)memdup(d, sz, 1);
        return {res, size()};
    }
};

// TODO: rename StrSlice and add WStrSlice

namespace str {

// a class to help scanning through text. doesn't own the data
struct Slice {
    char* begin = nullptr;
    char* end = nullptr;
    char* curr = nullptr;

    Slice() = default;
    Slice(char* s, size_t len);
    Slice(char* start, char* end);
    Slice(const Slice& other);
    Slice& operator=(const Slice&);

    void Set(char* s, size_t len);

    size_t Left() const;
    bool Finished() const;

    char PrevChar() const;
    char CurrChar() const;
    size_t AdvanceCurrTo(char* s);
    size_t SkipWsUntilNewline();
    size_t SkipUntil(char toFind);
    size_t SkipNonWs();
    size_t Skip(int n);
    void ZeroCurr() const;
};

} // namespace str
