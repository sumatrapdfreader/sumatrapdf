#ifndef SerializeTxtParser_h
#define SerializeTxtParser_h

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

    void Init(char *txt, size_t len) {
        begin = txt;
        curr = txt;
        end = txt + len;
    }

    bool Finished() { return begin >= end; }
};

} // namespace str

struct ParsedEl {
    ParsedEl *      next;
    ParsedEl *      child;
    const char *    s;
};

struct TxtParser {
    Allocator *     allocator;
    ParsedEl *      root;
    str::Slice      s;

    TxtParser() {
        allocator = new PoolAllocator();
        root = NULL;
    }
};

bool ParseTxt(TxtParser& parser);

#endif
