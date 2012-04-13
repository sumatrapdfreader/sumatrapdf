/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookBase_h
#define EbookBase_h

// this is a collection of structs and classes that are
// useful for more than one ebook format

struct ImageData {
    char *      data;
    size_t      len;
};

class EbookTocVisitor {
public:
    virtual void visit(const TCHAR *name, const TCHAR *url, int level) = 0;
};

#endif
