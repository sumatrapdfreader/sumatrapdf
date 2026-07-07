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

namespace mui {
struct CachedFont;
class ITextRender;
} // namespace mui
#endif

enum class PlatformFontStyle {
    Regular = 0,
    Bold = 1,
    Italic = 2,
    Underline = 4,
    Strikeout = 8,
};

inline PlatformFontStyle operator|(PlatformFontStyle a, PlatformFontStyle b) {
    return (PlatformFontStyle)((int)a | (int)b);
}

struct PlatformFont {
    WStr name;
    float sizePt = 0;
    PlatformFontStyle style = PlatformFontStyle::Regular;
#if OS_WIN
    mui::CachedFont* cachedFont = nullptr;
#endif

    WStr GetName() const { return name; }
    float GetSize() const { return sizePt; }
    PlatformFontStyle GetStyle() const { return style; }
#if OS_WIN
    mui::CachedFont* GetCachedFont() const { return cachedFont; }
#endif
};

enum class PlatformTextMeasureMethod {
    Gdiplus,
    GdiplusQuick,
    Gdi,
    Hdc,
    Stub,
};

struct PlatformTextMeasurer {
    virtual void SetFont(PlatformFont* font) = 0;
    virtual float GetCurrFontLineSpacing() = 0;
    virtual float GetSpaceDx() = 0;
    virtual RectF Measure(WStr s) = 0;
    virtual int StringLenForWidth(WStr s, float dx, float sWidth = -1) = 0;
    virtual ~PlatformTextMeasurer() = default;
};

PlatformFont* GetPlatformFont(WStr name, float sizePt, PlatformFontStyle style);
PlatformTextMeasurer* CreatePlatformTextMeasurer(PlatformTextMeasureMethod method);

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
    // an image (raw data for e.g. PixmapFromData)
    Image,
    // marks the beginning of a link (<a> tag)
    LinkStart,
    // marks end of the link (must have matching InstrLinkStart)
    LinkEnd,
    // marks an anchor an internal link might refer to
    Anchor,
    // an Anchor that marks the beginning of a sub-document within
    // a merged document (str is the sub-document's path)
    PageMarkerAnchor,
    // same as InstrString but for RTL text
    RtlString,
};

struct DrawInstr {
    DrawInstrType type{DrawInstrType::Unknown};
    // info specific to a given instruction
    // InstrString, InstrLinkStart, InstrAnchor, InstrRtlString, InstrImage
    ::Str str;
    PlatformFont* font = nullptr; // InstrSetFont
    RectF bbox{};                 // common to most instructions

    DrawInstr() = default;

    explicit DrawInstr(DrawInstrType t, RectF bbox = {}) : type(t), bbox(bbox) {}
    Str GetImage() {
        ReportIf(type != DrawInstrType::Image);
        return Str((char*)str.s, (int)str.len);
    }

    // helper constructors for instructions that need additional arguments
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

    StyleRule() = default;

    void Merge(StyleRule& source);

    static StyleRule Parse(CssPullParser* parser);
    static StyleRule Parse(::Str s);
};

struct DrawStyle {
    PlatformFont* font = nullptr;
    AlignAttr align{AlignAttr::NotFound};
    bool dirRtl = false;
};

struct IPageElement;

struct HtmlPage {
    explicit HtmlPage(int reparseIdx = 0) : reparseIdx(reparseIdx) {}

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
    ~HtmlFormatterArgs() { wstr::Free(fontName); }

    float pageDx = 0;
    float pageDy = 0;

    void SetFontName(WStr s) {
        wstr::Free(fontName);
        fontName = wstr::Dup(s);
    }

    WStr GetFontName() const { return fontName; }

    float fontSize = 0;

    /* Strings stored in DrawInstr must outlive the formatter (they are
       used for the lifetime of the engine). Strings that don't point into
       the original html text (e.g. resolved html entities or attribute
       values, which are owned by the gumbo parse tree destroyed with the
       formatter) are copied into this allocator. */
    Arena* textAllocator = nullptr;

    PlatformTextMeasureMethod textRenderMethod = PlatformTextMeasureMethod::Gdiplus;

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
    bool HandleTagA(HtmlToken* t, ::Str linkAttr = StrL("href"), ::Str attrNS = ::Str());
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
    void HandleText(::Str s);
    // blank convenience methods to override
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
    void UpdateLinkBboxes(HtmlPage* page);

    bool EmitImage(Str img);
    void EmitHr();
    void EmitTextRun(::Str s);
    // emits a synthetic, persistent string (e.g. a list bullet/number)
    void EmitTextMarker(::Str s);
    void EmitElasticSpace();
    void EmitParagraph(float indent);
    void EmitEmptyLine(float lineDy);
    void EmitNewPage();
    void ForceNewPage();
    bool EnsureDx(float dx);

    DrawStyle* CurrStyle() { return &styleStack.Last(); }
    PlatformFont* CurrFont() { return CurrStyle()->font; }
    void SetFont(WStr fontName, PlatformFontStyle fs, float fontSize = -1);
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

    RectF MeasureTextCached(WStr s);

    void DumpLineDebugInfo();

    // constant during layout process
    float pageDx = 0;
    float pageDy = 0;
    float lineSpacing = 0;
    float spaceDx = 0;
    WStr defaultFontName;
    float defaultFontSize = 0;
    Arena* textAllocator = nullptr;
    PlatformTextMeasurer* textMeasure = nullptr;

    // Cache of measured text. We assume few distinct fonts, so each font gets
    // its own hash table (keyed by text only). If we ever see more than
    // kMaxMeasureCacheFonts fonts, further fonts measure uncached. Because
    // measurements come in runs of the same font, we remember the last font
    // to skip the per-font table lookup.
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
    // per-open-list marker state, for <ul> bullets and <ol> numbering
    // (incl. honoring the <ol start="N"> attribute)
    struct ListInfo {
        bool ordered = false;
        int nextNum = 1;
    };
    Vec<ListInfo> listInfos;
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

    GumboHtmlParser* htmlParser = nullptr;

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

#if OS_WIN
void DrawHtmlPage(Gdiplus::Graphics* g, mui::ITextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX,
                  float offY, bool showBbox, Gdiplus::Color textColor, bool* abortCookie = nullptr);
#endif

PlatformTextMeasureMethod GetTextRenderMethod();
void SetTextRenderMethod(PlatformTextMeasureMethod method);
HtmlFormatterArgs* CreateFormatterDefaultArgs(int dx, int dy, Arena* textAllocator = nullptr);
