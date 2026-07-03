/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* formatting extensions for Mobi */

struct MobiDoc;

struct MobiFormatter : HtmlFormatter {
    // accessor to images (and other format-specific data)
    // it can be nullptr (enables testing by feeding raw html)
    MobiDoc* doc;

    void HandleSpacing_Mobi(HtmlToken* t);
    void HandleTagImg(HtmlToken* t) override;
    void HandleHtmlTag(HtmlToken* t) override;

  public:
    MobiFormatter(HtmlFormatterArgs* args, MobiDoc* doc);
};

/* formatting extensions for EPUB */

struct EpubDoc;

struct EpubFormatter : HtmlFormatter {
    void HandleTagImg(HtmlToken* t) override;
    void HandleTagPagebreak(HtmlToken* t) override;
    void HandleTagLink(HtmlToken* t) override;
    void HandleHtmlTag(HtmlToken* t) override;
    bool IgnoreText() override;

    void HandleTagSvgImage(HtmlToken* t);

    EpubDoc* epubDoc;
    Str pagePath;
    int hiddenDepth;

  public:
    EpubFormatter(HtmlFormatterArgs* args, EpubDoc* doc) : HtmlFormatter(args), epubDoc(doc), hiddenDepth(0) {}
    ~EpubFormatter() override;
};

/* formatting extensions for FictionBook */

struct Fb2Doc;

struct Fb2Formatter : HtmlFormatter {
    int section;
    int titleCount;

    void HandleTagImg(HtmlToken* t) override;
    void HandleTagAsHtml(HtmlToken* t, Str name);
    void HandleHtmlTag(HtmlToken* t) override;

    bool IgnoreText() override { return false; }

    Fb2Doc* fb2Doc;

    Fb2Formatter(HtmlFormatterArgs* args, Fb2Doc* doc);
};

/* formatting extensions for standalone HTML */

struct HtmlDoc;

struct HtmlFileFormatter : HtmlFormatter {
    void HandleTagImg(HtmlToken* t) override;
    void HandleTagLink(HtmlToken* t) override;

    HtmlDoc* htmlDoc;

    HtmlFileFormatter(HtmlFormatterArgs* args, HtmlDoc* doc) : HtmlFormatter(args), htmlDoc(doc) {}
};

/* formatting extensions for TXT */

struct TxtFormatter : HtmlFormatter {
    void HandleTagPagebreak(HtmlToken*) override { ForceNewPage(); }

    explicit TxtFormatter(HtmlFormatterArgs* args) : HtmlFormatter(args) {}
};
