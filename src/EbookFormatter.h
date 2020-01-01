/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* formatting extensions for Mobi */

class MobiDoc;

class MobiFormatter : public HtmlFormatter {
    // accessor to images (and other format-specific data)
    // it can be nullptr (enables testing by feeding raw html)
    MobiDoc* doc;

    void HandleSpacing_Mobi(HtmlToken* t);
    virtual void HandleTagImg(HtmlToken* t);
    virtual void HandleHtmlTag(HtmlToken* t);

  public:
    MobiFormatter(HtmlFormatterArgs* args, MobiDoc* doc);
};

/* formatting extensions for EPUB */

class EpubDoc;

class EpubFormatter : public HtmlFormatter {
    virtual void HandleTagImg(HtmlToken* t);
    virtual void HandleTagPagebreak(HtmlToken* t);
    virtual void HandleTagLink(HtmlToken* t);
    virtual void HandleHtmlTag(HtmlToken* t);
    virtual bool IgnoreText();

    void HandleTagSvgImage(HtmlToken* t);

    EpubDoc* epubDoc;
    AutoFree pagePath;
    size_t hiddenDepth;

  public:
    EpubFormatter(HtmlFormatterArgs* args, EpubDoc* doc) : HtmlFormatter(args), epubDoc(doc), hiddenDepth(0) {
    }
};

/* formatting extensions for FictionBook */

class Fb2Doc;

class Fb2Formatter : public HtmlFormatter {
    int section;
    int titleCount;

    virtual void HandleTagImg(HtmlToken* t);
    void HandleTagAsHtml(HtmlToken* t, const char* name);
    virtual void HandleHtmlTag(HtmlToken* t);

    virtual bool IgnoreText() {
        return false;
    }

    Fb2Doc* fb2Doc;

  public:
    Fb2Formatter(HtmlFormatterArgs* args, Fb2Doc* doc);
};

/* formatting extensions for standalone HTML */

class HtmlDoc;

class HtmlFileFormatter : public HtmlFormatter {
  protected:
    virtual void HandleTagImg(HtmlToken* t);
    virtual void HandleTagLink(HtmlToken* t);

    HtmlDoc* htmlDoc;

  public:
    HtmlFileFormatter(HtmlFormatterArgs* args, HtmlDoc* doc) : HtmlFormatter(args), htmlDoc(doc) {
    }
};

/* formatting extensions for TXT */

class TxtFormatter : public HtmlFormatter {
  protected:
    virtual void HandleTagPagebreak(HtmlToken* t) {
        UNUSED(t);
        ForceNewPage();
    }

  public:
    explicit TxtFormatter(HtmlFormatterArgs* args) : HtmlFormatter(args) {
    }
};
