#ifndef StrSlice_h
#define StrSlice_h

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
    int SkipWsUntilNewline();
    int SkipUntil(char toFind);
    int SkipNonWs();
    int Skip(int n);
    void ZeroCurr();
};

} // namespace str

#endif
