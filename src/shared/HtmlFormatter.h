/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace dict {
class MapStrToInt;
}

#if OS_WIN
namespace Gdiplus {
class Color;
class Graphics;
} // namespace Gdiplus
#endif

// Include gui/PlatformFont.h and gui/PlatformText.h first.
enum class DrawInstrType {
    Unknown = 0,
    String = 1,
    // Minimum spaceDx; expands during justification.
    ElasticSpace,
    // Fixed width, e.g. paragraph indentation.
    FixedSpace,
    Line,
    SetFont,
    Image,
    LinkStart,
    LinkEnd,
    Anchor,
    // Sub-document boundary; str holds its path.
    PageMarkerAnchor,
    RtlString,
};

struct DrawInstr {
    DrawInstrType type{DrawInstrType::Unknown};
    // Text, link target, anchor name or encoded image.
    ::Str str;
    PlatformFont* font = nullptr;
    RectF bbox{};

    DrawInstr() = default;

    explicit DrawInstr(DrawInstrType t, RectF bbox = {}, ::Str s = {}) : type(t), str(s), bbox(bbox) {}
    Str GetImage() {
        ReportIf(type != DrawInstrType::Image);
        return Str(str.s, str.len);
    }

    static DrawInstr Text(::Str s, RectF bbox, bool rtl = false);
    static DrawInstr Image(Str, RectF bbox);
    static DrawInstr SetFont(PlatformFont* font);
    static DrawInstr FixedSpace(float dx);
    static DrawInstr LinkStart(::Str s);
    static DrawInstr Anchor(::Str s, RectF bbox);
    static DrawInstr PageMarkerAnchor(::Str s, RectF bbox);
};

class CssPullParser;

struct StyleRule {
    HtmlTag tag = Tag_NotFound;
    u32 classHash = 0;

    enum Unit {
        px,
        pt,
        em,
        inherit
    };

    float textIndent = 0;
    Unit textIndentUnit = inherit;
    AlignAttr textAlign = AlignAttr::NotFound;

    void Merge(StyleRule& source);

    static StyleRule Parse(CssPullParser* parser);
    static StyleRule Parse(::Str s);
};

void ParseSizeWithUnit(Str s, float* size, StyleRule::Unit* unit);

struct DrawStyle {
    PlatformFont* font = nullptr;
    AlignAttr align{AlignAttr::NotFound};
    bool dirRtl = false;
};

struct IPageElement;

struct HtmlPage {
    explicit HtmlPage(int reparseIdx = 0) : reparseIdx(reparseIdx) {}

    Vec<DrawInstr> instructions;
    // HTML offset for reparsing. Restarting here may not recover the original style.
    int reparseIdx;

    Vec<IPageElement*> elements;
    bool gotElements = false;
};

struct HtmlFormatterArgs {
    ~HtmlFormatterArgs() { wstr::Free(fontName); }

    float pageDx = 0;
    float pageDy = 0;

    void SetFontName(WStr s) {
        wstr::Free(fontName);
        fontName = wstr::Dup(s);
    }

    WStr GetFontName() const { return fontName; }

    float fontSize = 0;
    bool overrideFontName = false;

    // DrawInstr strings must outlive the formatter. Keep original HTML alive with
    // the pages; copy generated strings and Gumbo attributes into this arena.
    Arena* textAllocator = nullptr;

    Str htmlStr;

    // we start parsing from htmlStr + reparseIdx
    int reparseIdx = 0;

    WStr fontName;
};

class GumboHtmlParser;
struct HtmlToken;
struct CssSelector;

struct HtmlFormatter {
  protected:
    void HandleTagBr();
    void HandleTagP(HtmlToken* t, bool isDiv = false);
    void HandleTagFont(HtmlToken* t);
    bool HandleTagA(HtmlToken* t, ::Str linkAttr = StrL("href"), HtmlNameMatch match = HtmlNameMatch::Exact);
    void HandleTagHx(HtmlToken* t);
    void HandleTagList(HtmlToken* t);
    void HandleTagPre(HtmlToken* t);
    void HandleTagStyle(HtmlToken* t);

    void HandleAnchorAttr(HtmlToken* t, bool idsOnly = false);
    void HandleDirAttr(HtmlToken* t);

    void AutoCloseTags(size_t count);
    void UpdateTagNesting(HtmlToken* t);
    virtual void HandleHtmlTag(HtmlToken* t);
    void HandleText(::Str s);
    virtual void HandleTagImg(HtmlToken* t) {}
    virtual void HandleTagPagebreak(HtmlToken*) {}
    virtual void HandleTagLink(HtmlToken*) {}

    float CurrLineDx();
    float CurrLineDy();
    float NewLineX() const;
    void LayoutLeftStartingAt(float offX);
    void JustifyLineBoth();
    void JustifyCurrLine(AlignAttr align);
    bool FlushCurrLine(bool isParagraphBreak);

    bool EmitImage(Str img);
    void EmitImageOrAlt(HtmlToken* t, Str img);
    void EmitHr();
    void EmitTextRun(::Str s);
    void EmitTextMarker(::Str s);
    void EmitElasticSpace();
    void EmitParagraph(float indent);
    void EmitEmptyLine(float lineDy);
    void EmitNewPage();
    void ForceNewPage();
    bool EnsureDx(float dx);

    DrawStyle* CurrStyle() { return &VecLast(styleStack); }
    PlatformFont* CurrFont() { return CurrStyle()->font; }
    void SetFont(Str fontName, PlatformFontStyle fs, float fontSize = -1);
    void SetFontBasedOn(PlatformFont* origFont, PlatformFontStyle fs, float fontSize = -1);
    void ChangeFontStyle(PlatformFontStyle fs, bool addStyle);
    void SetAlignment(AlignAttr align);
    void RevertStyleChange();

    void ParseStyleSheet(::Str data);
    StyleRule* FindStyleRule(HtmlTag tag, ::Str clazz);
    StyleRule ComputeStyleRule(HtmlToken* t);

    void AppendInstr(const DrawInstr& di);
    bool IsCurrLineEmpty();
    virtual bool IgnoreText();

    RectF MeasureTextCached(Str s);

    float pageDx = 0;
    float pageDy = 0;
    float lineSpacing = 0;
    float spaceDx = 0;
    Str defaultFontName;
    float defaultFontSize = 0;
    bool overrideFontName = false;
    Arena* textAllocator = nullptr;
    PlatformTextRender* textMeasure = nullptr;

    // Cache by font and text; excess fonts measure uncached.
    // Remember the last font to avoid repeated table lookups.
    static constexpr int kMaxMeasureCacheFonts = 6;
    struct MeasureCache {
        PlatformFont* font = nullptr;
        dict::MapStrToInt* keys = nullptr; // text -> index into vals
        Vec<RectF> vals;
    };
    MeasureCache measureCaches[kMaxMeasureCacheFonts];
    int nMeasureCaches = 0;
    int measureCacheInitialSize = 1024;
    MeasureCache* lastMeasureCache = nullptr;

    MeasureCache* GetMeasureCacheForCurrFont();

    // style stack of the current line
    Vec<DrawStyle> styleStack;
    PlatformFont* nextPageFont = nullptr;
    float currX = 0;
    float currY = 0;
    // Deferred top padding applied when the line is flushed.
    float currLineTopPadding = 0;
    int listDepth = 0;
    // Markers for open lists, including <ol start="N">.
    struct ListInfo {
        bool ordered = false;
        int nextNum = 1;
    };
    Vec<ListInfo> listInfos;
    bool preFormatted = false;
    bool dirRtl = false;
    // list of currently opened tags for auto-closing when needed
    Vec<HtmlTag> tagNesting;
    bool keepTagNesting = false;
    Vec<StyleRule> styleRules;

    Vec<DrawInstr> currLineInstr;
    // HTML offset of the line's first instruction.
    ptrdiff_t currLineReparseIdx = 0;
    HtmlPage* currPage = nullptr;

    // One-based LinkStart index in currLineInstr; zero outside a link.
    size_t currLinkIdx = 0;

    // Current token's HTML offset.
    ptrdiff_t currReparseIdx = 0;

    GumboHtmlParser* htmlParser = nullptr;

    // Pages awaiting Next().
    Vec<HtmlPage*> pagesToSend;

    bool finishedParsing = false;

  public:
    explicit HtmlFormatter(HtmlFormatterArgs* args);
    HtmlFormatter(HtmlFormatter const&) = delete;
    HtmlFormatter& operator=(HtmlFormatter const&) = delete;
    virtual ~HtmlFormatter();

    HtmlPage* Next(bool skipEmptyPages = true);
    Vec<HtmlPage*>* FormatAllPages(bool skipEmptyPages = true);
};

#if OS_WIN
void DrawHtmlPage(Gdiplus::Graphics* g, PlatformTextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX,
                  float offY, bool showBbox, Color textColor, bool* abortCookie = nullptr);
#elif OS_LINUX
struct _cairo;
void DrawHtmlPage(struct _cairo* cairo, PlatformTextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX,
                  float offY, bool showBbox, Color textColor, bool* abortCookie = nullptr);
#elif OS_DARWIN
struct CGContext;
void DrawHtmlPage(struct CGContext* context, PlatformTextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX,
                  float offY, bool showBbox, Color textColor, bool* abortCookie = nullptr);
#endif

HtmlFormatterArgs* CreateFormatterDefaultArgs(int dx, int dy, Arena* textAllocator = nullptr);
