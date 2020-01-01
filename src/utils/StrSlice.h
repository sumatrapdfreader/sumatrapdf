/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace str {

// a class to help scanning through text. doesn't own the edata
struct Slice {
    char* begin = nullptr;
    char* end = nullptr;
    char* curr = nullptr;

    Slice();
    Slice(char* s, size_t len);
    Slice(char* start, char* end);
    Slice(const Slice& other);

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
    void ZeroCurr();
};

} // namespace str
