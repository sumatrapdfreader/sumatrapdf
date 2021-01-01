/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* formatting extensions for Mobi */

class MobiDoc;

class MobiFormatter : public HtmlFormatter {
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

class EpubDoc;

class EpubFormatter : public HtmlFormatter {
    void HandleTagImg(HtmlToken* t) override;
    void HandleTagPagebreak(HtmlToken* t) override;
    void HandleTagLink(HtmlToken* t) override;
    void HandleHtmlTag(HtmlToken* t) override;
    bool IgnoreText() override;

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

    void HandleTagImg(HtmlToken* t) override;
    void HandleTagAsHtml(HtmlToken* t, const char* name);
    void HandleHtmlTag(HtmlToken* t) override;

    bool IgnoreText() override {
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
    void HandleTagImg(HtmlToken* t) override;
    void HandleTagLink(HtmlToken* t) override;

    HtmlDoc* htmlDoc;

  public:
    HtmlFileFormatter(HtmlFormatterArgs* args, HtmlDoc* doc) : HtmlFormatter(args), htmlDoc(doc) {
    }
};

/* formatting extensions for TXT */

class TxtFormatter : public HtmlFormatter {
  protected:
    void HandleTagPagebreak([[maybe_unused]] HtmlToken* t) override {
        ForceNewPage();
    }

  public:
    explicit TxtFormatter(HtmlFormatterArgs* args) : HtmlFormatter(args) {
    }
};
