/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace str {

// a class to help scanning through text
struct Slice {
    char *  begin;
    char *  end;
    char *  curr;

    Slice() : begin(NULL), end(NULL), curr(NULL) { }

    Slice(char *txt, size_t len) {
        Init(txt, len);
    }

    Slice(char *start, char *end) {
        Init(start, end-start);
    }

    Slice(const Slice& other) {
        this->begin = other.begin;
        this->end = other.end;
        this->curr = other.curr;
    }

    void Init(char *txt, size_t len) {
        begin = txt;
        curr = txt;
        end = txt + len;
    }

    bool Finished() const { return curr >= end; }

    char PrevChar() const;
    char CurrChar() const;
    ptrdiff_t SkipWsUntilNewline();
    ptrdiff_t SkipUntil(char toFind);
    ptrdiff_t SkipNonWs();
    ptrdiff_t Skip(int n);
    void ZeroCurr();
};

} // namespace str
