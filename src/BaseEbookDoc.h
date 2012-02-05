/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

#ifndef BaseEbookDoc_h
#define BaseEbookDoc_h

class BaseEbookDoc {
public:
    virtual ~BaseEbookDoc() { }
    // returns the document's content converted into
    // a single HTML stream for further processing/layout
    // the result is owned by the class, don't free
    virtual const char *GetBookHtmlData(size_t& lenOut) = 0;
};

#endif
