#ifndef __CRTRACE
#define __CRTRACE 1

#include "lvstring.h"

struct endtrace {
    endtrace() {}
};

class crtrace {
    lString8 buffer_;
public:
    crtrace() : buffer_()  {}
    crtrace(const char *c) : buffer_(c) {}
    virtual ~crtrace() { flush(); }
    void flush() {
        CRLog::info(buffer_.c_str());
        buffer_.clear();
    }

    crtrace& operator << (const char *s) {
        buffer_.append(s);
        return *this;
    }

    crtrace& operator << (const lString8& ls8) {
        buffer_.append(ls8);
        return *this;
    }

    crtrace& operator << (const lString16& ls16) {
        buffer_.append(UnicodeToUtf8(ls16));
        return *this;
    }

    crtrace& operator << (int i) {
        buffer_.append(lString8::itoa(i));
        return *this;
    }

    void operator << (const endtrace&) {
        flush();
    }

};

#endif
