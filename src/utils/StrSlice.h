/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
    void ZeroCurr();
};

} // namespace str
