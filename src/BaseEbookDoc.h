/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

#ifndef BaseEbookDoc_h
#define BaseEbookDoc_h

#include "BaseUtil.h"

// TODO: use Gdiplus::Bitmap instead?
struct ImageData {
    char *  data;
    size_t  len;
    char *  id;
};

class BaseEbookDoc {
public:
    virtual ~BaseEbookDoc() { }
    // returns the document's path
    // the result is owned by the class, don't free
    virtual const TCHAR *GetFilepath() = 0;
    // returns the document's content converted into
    // a single HTML stream for further processing/layout
    // the result is owned by the class, don't free
    virtual const char *GetBookHtmlData(size_t& lenOut) = 0;
    // returns the data for an image by ID
    // the result is owned by the class, don't free
    virtual ImageData *GetImageData(const char *id) { return NULL; }
    // returns the data for an image by (arbitrary) index
    // the result is owned by the class, don't free
    virtual ImageData *GetImageData(size_t index) { return NULL; }
};

#endif
