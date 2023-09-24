/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::FontFamily;
using Gdiplus::FontStyleBold;
using Gdiplus::FontStyleItalic;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleStrikeout;
using Gdiplus::FontStyleUnderline;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::UnitPixel;
using Gdiplus::Win32Error;

// Layout information for a given page is a list of
// draw instructions that define what to draw and where.
enum class DrawInstrType {
    Unknown = 0,
    // a piece of text
    String = 1,
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
    DrawInstrType type{DrawInstrType::Unknown};
    union {
        // info specific to a given instruction
        // InstrString, InstrLinkStart, InstrAnchor, InstrRtlString, InstrImage
        struct {
            const char* s;
            size_t len;
        } str{nullptr, 0};
        mui::CachedFont* font; // InstrSetFont
    };
    RectF bbox{}; // common to most instructions

    DrawInstr() = default;

    explicit DrawInstr(DrawInstrType t, RectF bbox = {}) : type(t), bbox(bbox) {
    }
    ByteSlice GetImage() {
        CrashIf(type != DrawInstrType::Image);
        return {(u8*)str.s, str.len};
    }

    // helper constructors for instructions that need additional arguments
    static DrawInstr Str(const char* s, size_t len, RectF bbox, bool rtl = false);
    static DrawInstr Image(const ByteSlice&, RectF bbox);
    static DrawInstr SetFont(mui::CachedFont* font);
    static DrawInstr FixedSpace(float dx);
    static DrawInstr LinkStart(const char* s, size_t len);
    static DrawInstr Anchor(const char* s, size_t len, RectF bbox);
};

class CssPullParser;

struct StyleRule {
    HtmlTag tag = Tag_NotFound;
    u32 classHash = 0;

    enum Unit { px, pt, em, inherit };

    float textIndent = 0;
    Unit textIndentUnit = inherit;
    AlignAttr textAlign = AlignAttr::NotFound;

    StyleRule() = default;

    void Merge(StyleRule& source);

    static StyleRule Parse(CssPullParser* parser);
    static StyleRule Parse(const char* s, size_t len);
};

struct DrawStyle {
    mui::CachedFont* font = nullptr;
    AlignAttr align{AlignAttr::NotFound};
    bool dirRtl = false;
};

struct IPageElement;

struct HtmlPage {
    explicit HtmlPage(int reparseIdx = 0) : reparseIdx(reparseIdx) {
    }

    Vec<DrawInstr> instructions;
    // if we start parsing html again from reparseIdx, we should
    // get the same instructions. reparseIdx is an offset within
    // html data
    // TODO: reparsing from reparseIdx can lead to different styling
    // due to internal state of HtmlFormatter not being properly set
    int reparseIdx;

    Vec<IPageElement*> elements;
    bool gotElements = false;
};

// just to pack args to HtmlFormatter
struct HtmlFormatterArgs {
    HtmlFormatterArgs() = default;

    float pageDx = 0;
    float pageDy = 0;

    void SetFontName(const WCHAR* s) {
        fontName.SetCopy(s);
    }

    const WCHAR* GetFontName() const {
        return fontName;
    }

    float fontSize = 0;

    /* Most of the time string DrawInstr point to original html text
       that is read-only and outlives us. Sometimes (e.g. when resolving
       html entities) we need a modified text. This allocator is
       used to allocate this text. */
    Allocator* textAllocator = nullptr;

    mui::TextRenderMethod textRenderMethod = mui::TextRenderMethod::Gdiplus;

    ByteSlice htmlStr;

    // we start parsing from htmlStr + reparseIdx
    int reparseIdx = 0;

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
    }
    virtual void HandleTagPagebreak(HtmlToken*) {
    }
    virtual void HandleTagLink(HtmlToken*) {
    }

    float CurrLineDx();
    float CurrLineDy();
    float NewLineX() const;
    void LayoutLeftStartingAt(float offX);
    void JustifyLineBoth();
    void JustifyCurrLine(AlignAttr align);
    bool FlushCurrLine(bool isParagraphBreak);
    void UpdateLinkBboxes(HtmlPage* page);

    bool EmitImage(const ByteSlice* img);
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
    void ChangeFontStyle(FontStyle fs, bool addStyle);
    void SetAlignment(AlignAttr align);
    void RevertStyleChange();

    void ParseStyleSheet(const char* data, size_t len);
    StyleRule* FindStyleRule(HtmlTag tag, const char* clazz, size_t clazzLen);
    StyleRule ComputeStyleRule(HtmlToken* t);

    void AppendInstr(const DrawInstr& di);
    bool IsCurrLineEmpty();
    virtual bool IgnoreText();

    void DumpLineDebugInfo();

    // constant during layout process
    float pageDx = 0;
    float pageDy = 0;
    float lineSpacing = 0;
    float spaceDx = 0;
    Graphics* gfx = nullptr; // for measuring text
    AutoFreeWstr defaultFontName;
    float defaultFontSize = 0;
    Allocator* textAllocator = nullptr;
    mui::ITextRender* textMeasure = nullptr;

    // style stack of the current line
    Vec<DrawStyle> styleStack;
    // style for the start of the next page
    DrawStyle nextPageStyle;
    // current position in a page
    float currX = 0;
    float currY = 0;
    // remembered when we start a new line, used when we actually
    // layout a line
    float currLineTopPadding = 0;
    // number of nested lists for indenting whole paragraphs
    int listDepth = 0;
    // set if newlines are not to be ignored
    bool preFormatted = false;
    // set if the reading direction is RTL
    bool dirRtl = false;
    // list of currently opened tags for auto-closing when needed
    Vec<HtmlTag> tagNesting;
    bool keepTagNesting = false;
    // set from CSS and to be checked by the individual tag handlers
    Vec<StyleRule> styleRules;

    // isntructions for the current line
    Vec<DrawInstr> currLineInstr;
    // reparse point of the first instructions in a current line
    ptrdiff_t currLineReparseIdx = 0;
    HtmlPage* currPage = nullptr;

    // for tracking whether we're currently inside <a> tag
    size_t currLinkIdx = 0;

    // reparse point for the current HtmlToken
    ptrdiff_t currReparseIdx = 0;

    HtmlPullParser* htmlParser = nullptr;

    // list of pages that we've created but haven't yet sent to client
    Vec<HtmlPage*> pagesToSend;

    bool finishedParsing = false;
    // number of pages generated so far, approximate. Only used
    // for detection of cover image duplicates in mobi formatting
    int pageCount = 0;

  public:
    explicit HtmlFormatter(HtmlFormatterArgs* args);
    HtmlFormatter(HtmlFormatter const&) = delete;
    HtmlFormatter& operator=(HtmlFormatter const&) = delete;
    virtual ~HtmlFormatter();

    HtmlPage* Next(bool skipEmptyPages = true);
    Vec<HtmlPage*>* FormatAllPages(bool skipEmptyPages = true);
};

void DrawHtmlPage(Graphics* g, mui::ITextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX, float offY,
                  bool showBbox, Color textColor, bool* abortCookie = nullptr);

mui::TextRenderMethod GetTextRenderMethod();
void SetTextRenderMethod(mui::TextRenderMethod method);
HtmlFormatterArgs* CreateFormatterDefaultArgs(int dx, int dy, Allocator* textAllocator = nullptr);
