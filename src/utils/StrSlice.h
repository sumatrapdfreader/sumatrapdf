/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct ByteSlice {
    u8* d = nullptr;
    size_t s = 0;

    ByteSlice() = default;
    ~ByteSlice() = default;
    ByteSlice(u8* data, size_t size) {
        d = data;
        s = size;
    }
    ByteSlice(const ByteSlice& data) {
        d = data.data();
        s = data.size();
    }
    ByteSlice& operator=(const ByteSlice& other) {
        d = other.d;
        s = other.s;
        return *this;
    }
    ByteSlice(const std::string_view& data) {
        d = (u8*)data.data();
        s = data.size();
    }
    u8* data() const {
        return d;
    }
    size_t size() const {
        return s;
    }
    bool empty() const {
        return !d || s == 0;
    }
    ByteSlice Clone() const {
        if (empty()) {
            return {};
        }
        u8* res = (u8*)memdup(d, s, 1);
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

    [[nodiscard]] size_t Left() const;
    [[nodiscard]] bool Finished() const;

    [[nodiscard]] char PrevChar() const;
    [[nodiscard]] char CurrChar() const;
    size_t AdvanceCurrTo(char* s);
    size_t SkipWsUntilNewline();
    size_t SkipUntil(char toFind);
    size_t SkipNonWs();
    size_t Skip(int n);
    void ZeroCurr() const;
};

} // namespace str
