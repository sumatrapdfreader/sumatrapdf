/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct ByteSlice {
    u8* d = nullptr;
    size_t sz = 0;
    u8* curr = nullptr;

    ByteSlice() = default;
    ~ByteSlice() = default;
    ByteSlice(const char* str) {
        d = (u8*)str;
        curr = d;
        sz = strlen(str);
    }
    ByteSlice(char* str) {
        d = (u8*)str;
        curr = d;
        sz = strlen(str);
    }
    ByteSlice(u8* data, size_t size) {
        d = data;
        curr = d;
        sz = size;
    }
    ByteSlice(const ByteSlice& data) {
        d = data.data();
        curr = d;
        sz = data.size();
    }
    ByteSlice& operator=(const ByteSlice& other) {
        d = other.d;
        curr = d;
        sz = other.sz;
        return *this;
    }
    void Set(u8* data, size_t size) {
        d = data;
        curr = d;
        sz = size;
    }
    void Set(char* data, size_t size) {
        d = (u8*)data;
        curr = d;
        sz = size;
    }
    u8* data() const {
        return d;
    }
    u8* Get() const {
        return d;
    }
    size_t size() const {
        return sz;
    }
    int Size() const {
        return (int)sz;
    }
    bool empty() const {
        return !d;
    }
    size_t Left() {
        return sz - (curr - d);
    }
    ByteSlice Clone() const {
        if (empty()) {
            return {};
        }
        u8* res = (u8*)memdup(d, sz, 1);
        return {res, size()};
    }
    void Free() {
        free(d);
        d = nullptr;
        sz = 0;
        curr = nullptr;
    }
    operator const char*() {
        return (const char*)d;
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
