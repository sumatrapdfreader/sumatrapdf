/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookFormatter_h
#define EbookFormatter_h

#include "HtmlFormatter.h"
#include "Doc.h"

HtmlFormatterArgs *CreateFormatterArgsDoc(Doc doc, int dx, int dy, PoolAllocator *textAllocator);
HtmlFormatter *CreateFormatter(Doc doc, HtmlFormatterArgs* args);

/* formatting extensions for Mobi */

class MobiDoc;

class MobiFormatter : public HtmlFormatter {
    // accessor to images (and other format-specific data)
    // it can be NULL (enables testing by feeding raw html)
    MobiDoc *           doc;
    // remember cover image if we've generated one, so that we
    // can avoid adding the same image twice if it's early in
    // the book
    ImageData *         coverImage;

    void HandleSpacing_Mobi(HtmlToken *t);
    virtual void HandleTagImg(HtmlToken *t);
    virtual void HandleHtmlTag(HtmlToken *t);

public:
    MobiFormatter(HtmlFormatterArgs *args, MobiDoc *doc);
};

/* formatting extensions for EPUB */

class EpubDoc;

class EpubFormatter : public HtmlFormatter {
protected:
    virtual void HandleTagImg(HtmlToken *t);
    virtual void HandleTagPagebreak(HtmlToken *t);
    virtual void HandleHtmlTag(HtmlToken *t);
    virtual bool IgnoreText();

    void HandleTagSvgImage(HtmlToken *t);

    EpubDoc *epubDoc;
    ScopedMem<char> pagePath;
    size_t hiddenDepth;

public:
    EpubFormatter(HtmlFormatterArgs *args, EpubDoc *doc) :
        HtmlFormatter(args), epubDoc(doc), hiddenDepth(0) { }
};

#endif
