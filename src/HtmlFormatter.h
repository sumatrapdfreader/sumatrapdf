/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

using namespace Gdiplus;

// Layout information for a given page is a list of
// draw instructions that define what to draw and where.
enum class DrawInstrType {
    // a piece of text
    String = 0,
    // elastic space takes at least spaceDx pixels but can take more
    // if a line is justified
    ElasticSpace,
    // a fixed space takes a fixed amount of pixels. It's used e.g.
    // to implement paragraph indentation
    FixedSpace,
    // a horizontal line
    Line,
    // change current font
    SetFont,
    // an image (raw data for e.g. BitmapFromData)
    Image,
    // marks the beginning of a link (<a> tag)
    LinkStart,
    // marks end of the link (must have matching InstrLinkStart)
    LinkEnd,
    // marks an anchor an internal link might refer to
    Anchor,
    // same as InstrString but for RTL text
    RtlString,
};

struct DrawInstr {
    DrawInstrType type;
    union {
        // info specific to a given instruction
        struct {
            const char* s;
            size_t len;
        } str;                 // InstrString, InstrLinkStart, InstrAnchor, InstrRtlString
        mui::CachedFont* font; // InstrSetFont
        ImageData img;         // InstrImage
    };
    RectF bbox; // common to most instructions

    DrawInstr() {
    }

    explicit DrawInstr(DrawInstrType t, RectF bbox = RectF()) : type(t), bbox(bbox) {
    }

    // helper constructors for instructions that need additional arguments
    static DrawInstr Str(const char* s, size_t len, RectF bbox, bool rtl = false);
    static DrawInstr Image(char* data, size_t len, RectF bbox);
    static DrawInstr SetFont(mui::CachedFont* font);
    static DrawInstr FixedSpace(float dx);
    static DrawInstr LinkStart(const char* s, size_t len);
    static DrawInstr Anchor(const char* s, size_t len, RectF bbox);
};

class CssPullParser;

struct StyleRule {
    HtmlTag tag;
    uint32_t classHash;

    enum Unit { px, pt, em, inherit };

    float textIndent;
    Unit textIndentUnit;
    AlignAttr textAlign;

    StyleRule();

    void Merge(StyleRule& source);

    static StyleRule Parse(CssPullParser* parser);
    static StyleRule Parse(const char* s, size_t len);
};

struct DrawStyle {
    mui::CachedFont* font;
    AlignAttr align;
    bool dirRtl;
};

class HtmlPage {
  public:
    explicit HtmlPage(int reparseIdx = 0) : reparseIdx(reparseIdx) {
        instructions.allowFailure = true;
    }

    Vec<DrawInstr> instructions;
    // if we start parsing html again from reparseIdx, we should
    // get the same instructions. reparseIdx is an offset within
    // html data
    // TODO: reparsing from reparseIdx can lead to different styling
    // due to internal state of HtmlFormatter not being properly set
    int reparseIdx;
};

// just to pack args to HtmlFormatter
class HtmlFormatterArgs {
  public:
    HtmlFormatterArgs() = default;

    REAL pageDx = 0;
    REAL pageDy = 0;

    void SetFontName(const WCHAR* s) {
        fontName.SetCopy(s);
    }

    const WCHAR* GetFontName() {
        return fontName;
    }

    float fontSize = 0;

    /* Most of the time string DrawInstr point to original html text
       that is read-only and outlives us. Sometimes (e.g. when resolving
       html entities) we need a modified text. This allocator is
       used to allocate this text. */
    Allocator* textAllocator = nullptr;

    mui::TextRenderMethod textRenderMethod = mui::TextRenderMethodGdiplus;

    std::string_view htmlStr;

    // we start parsing from htmlStr + reparseIdx
    int reparseIdx = 0;

  private:
    AutoFreeWstr fontName;
};

class HtmlPullParser;
struct HtmlToken;
struct CssSelector;

class HtmlFormatter {
  protected:
    void HandleTagBr();
    void HandleTagP(HtmlToken* t, bool isDiv = false);
    void HandleTagFont(HtmlToken* t);
    bool HandleTagA(HtmlToken* t, const char* linkAttr = "href", const char* attrNS = nullptr);
    void HandleTagHx(HtmlToken* t);
    void HandleTagList(HtmlToken* t);
    void HandleTagPre(HtmlToken* t);
    void HandleTagStyle(HtmlToken* t);

    void HandleAnchorAttr(HtmlToken* t, bool idsOnly = false);
    void HandleDirAttr(HtmlToken* t);

    void AutoCloseTags(size_t count);
    void UpdateTagNesting(HtmlToken* t);
    virtual void HandleHtmlTag(HtmlToken* t);
    void HandleText(HtmlToken* t);
    void HandleText(const char* s, size_t sLen);
    // blank convenience methods to override
    virtual void HandleTagImg(HtmlToken* t) {
        UNUSED(t);
    }
    virtual void HandleTagPagebreak(HtmlToken* t) {
        UNUSED(t);
    }
    virtual void HandleTagLink(HtmlToken* t) {
        UNUSED(t);
    }

    float CurrLineDx();
    float CurrLineDy();
    float NewLineX();
    void LayoutLeftStartingAt(REAL offX);
    void JustifyLineBoth();
    void JustifyCurrLine(AlignAttr align);
    bool FlushCurrLine(bool isParagraphBreak);
    void UpdateLinkBboxes(HtmlPage* page);

    bool EmitImage(ImageData* img);
    void EmitHr();
    void EmitTextRun(const char* s, const char* end);
    void EmitElasticSpace();
    void EmitParagraph(float indent);
    void EmitEmptyLine(float lineDy);
    void EmitNewPage();
    void ForceNewPage();
    bool EnsureDx(float dx);

    DrawStyle* CurrStyle() {
        return &styleStack.Last();
    }
    mui::CachedFont* CurrFont() {
        return CurrStyle()->font;
    }
    void SetFont(const WCHAR* fontName, FontStyle fs, float fontSize = -1);
    void SetFontBasedOn(mui::CachedFont* origFont, FontStyle fs, float fontSize = -1);
    void ChangeFontStyle(FontStyle fs, bool isStart);
    void SetAlignment(AlignAttr align);
    void RevertStyleChange();

    void ParseStyleSheet(const char* data, size_t len);
    StyleRule* FindStyleRule(HtmlTag tag, const char* clazz, size_t clazzLen);
    StyleRule ComputeStyleRule(HtmlToken* t);

    void AppendInstr(DrawInstr di);
    bool IsCurrLineEmpty();
    virtual bool IgnoreText();

    void DumpLineDebugInfo();

    // constant during layout process
    float pageDx;
    float pageDy;
    float lineSpacing;
    float spaceDx;
    Graphics* gfx; // for measuring text
    AutoFreeWstr defaultFontName;
    float defaultFontSize;
    Allocator* textAllocator;
    mui::ITextRender* textMeasure;

    // style stack of the current line
    Vec<DrawStyle> styleStack;
    // style for the start of the next page
    DrawStyle nextPageStyle;
    // current position in a page
    float currX, currY;
    // remembered when we start a new line, used when we actually
    // layout a line
    float currLineTopPadding;
    // number of nested lists for indenting whole paragraphs
    int listDepth;
    // set if newlines are not to be ignored
    bool preFormatted;
    // set if the reading direction is RTL
    bool dirRtl;
    // list of currently opened tags for auto-closing when needed
    Vec<HtmlTag> tagNesting;
    bool keepTagNesting;
    // set from CSS and to be checked by the individual tag handlers
    Vec<StyleRule> styleRules;

    // isntructions for the current line
    Vec<DrawInstr> currLineInstr;
    // reparse point of the first instructions in a current line
    ptrdiff_t currLineReparseIdx;
    HtmlPage* currPage;

    // for tracking whether we're currently inside <a> tag
    size_t currLinkIdx;

    // reparse point for the current HtmlToken
    ptrdiff_t currReparseIdx;

    HtmlPullParser* htmlParser;

    // list of pages that we've created but haven't yet sent to client
    Vec<HtmlPage*> pagesToSend;

    bool finishedParsing;
    // number of pages generated so far, approximate. Only used
    // for detection of cover image duplicates in mobi formatting
    int pageCount;

    WCHAR buf[512];

  public:
    explicit HtmlFormatter(HtmlFormatterArgs* args);
    virtual ~HtmlFormatter();

    HtmlPage* Next(bool skipEmptyPages = true);
    Vec<HtmlPage*>* FormatAllPages(bool skipEmptyPages = true);
};

void DrawHtmlPage(Graphics* g, mui::ITextRender* textRender, Vec<DrawInstr>* drawInstructions, REAL offX, REAL offY,
                  bool showBbox, Color textColor, bool* abortCookie = nullptr);

mui::TextRenderMethod GetTextRenderMethod();
void SetTextRenderMethod(mui::TextRenderMethod method);
HtmlFormatterArgs* CreateFormatterDefaultArgs(int dx, int dy, Allocator* textAllocator = nullptr);
