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

    void HandleSpacing_Mobi(HtmlToken *t);
    virtual void HandleTagImg(HtmlToken *t);
    virtual void HandleHtmlTag(HtmlToken *t);

public:
    MobiFormatter(HtmlFormatterArgs *args, MobiDoc *doc);
};

/* formatting extensions for EPUB */

class EpubDoc;

struct StyleRule {
    HtmlTag     tag;
    uint32_t    classHash;

    enum Unit { px, pt, em, inherit };

    float       textIndent;
    Unit        textIndentUnit;
    AlignAttr   textAlign;

    StyleRule() : tag(Tag_NotFound), classHash(0),
        textIndent(0), textIndentUnit(inherit), textAlign(Align_NotFound) { }
};

class EpubFormatter : public HtmlFormatter {
    virtual void HandleTagImg(HtmlToken *t);
    virtual void HandleTagPagebreak(HtmlToken *t);
    virtual void HandleHtmlTag(HtmlToken *t);
    virtual bool IgnoreText();

    void HandleTagStyle(HtmlToken *t);
    void HandleTagLink(HtmlToken *t);
    void HandleBlockStyling(HtmlToken *t);
    void HandleTagSvgImage(HtmlToken *t);

    EpubDoc *epubDoc;
    ScopedMem<char> pagePath;
    size_t hiddenDepth;
    Vec<StyleRule> styles;

public:
    EpubFormatter(HtmlFormatterArgs *args, EpubDoc *doc) :
        HtmlFormatter(args), epubDoc(doc), hiddenDepth(0) { }
};

/* formatting extensions for FictionBook */

#define FB2_TOC_ENTRY_MARK "ToC!Entry!"

class Fb2Doc;

class Fb2Formatter : public HtmlFormatter {
    int section;
    int titleCount;

    virtual void HandleTagImg(HtmlToken *t);
    void HandleTagAsHtml(HtmlToken *t, const char *name);
    virtual void HandleHtmlTag(HtmlToken *t);

    virtual bool IgnoreText() { return false; }

    Fb2Doc *fb2Doc;

public:
    Fb2Formatter(HtmlFormatterArgs *args, Fb2Doc *doc);
};

/* formatting extensions for PalmDOC */

class PalmDoc;

class PdbFormatter : public HtmlFormatter {
    PalmDoc *palmDoc;

public:
    PdbFormatter(HtmlFormatterArgs *args, PalmDoc *doc) :
        HtmlFormatter(args), palmDoc(doc) { }
};

/* formatting extensions for standalone HTML */

class HtmlDoc;

class HtmlFileFormatter : public HtmlFormatter {
protected:
    virtual void HandleTagImg(HtmlToken *t);
    virtual void HandleHtmlTag(HtmlToken *t);

    void HandleBlockStyling(HtmlToken *t);
    void HandleTagStyle(HtmlToken *t);
    void HandleTagLink(HtmlToken *t);

    HtmlDoc *htmlDoc;
    Vec<StyleRule> styles;

public:
    HtmlFileFormatter(HtmlFormatterArgs *args, HtmlDoc *doc) :
        HtmlFormatter(args), htmlDoc(doc) { }
};

/* formatting extensions for TXT */

class TxtFormatter : public HtmlFormatter {
protected:
    virtual void HandleTagPagebreak(HtmlToken *t) { ForceNewPage(); }

public:
    TxtFormatter(HtmlFormatterArgs *args) : HtmlFormatter(args) { }
};

#endif
