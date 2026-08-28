/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct fz_outline;
struct fz_link;
struct Pixmap;
struct RenderedBitmap;
struct IPageDestination;
struct TocItem;
struct PropValue;
enum class DocProp : u8;

struct ILinkHandler {
    virtual ~ILinkHandler() = default;
    virtual void GotoLink(IPageDestination*) = 0;
    virtual void GotoNamedDest(Str) = 0;
    virtual void GoToPage(int pageNo, bool addNavPoint) = 0;
    virtual bool GoToNextPage() = 0;
    virtual bool GoToPrevPage(bool toBottom = false) = 0;
    virtual void ScrollTo(IPageDestination*) = 0;
    virtual void ScrollTo(int pageNo, RectF rect, float zoom) = 0;
    virtual void LaunchURL(Str) = 0;
    virtual void LaunchFile(Str path, IPageDestination*) = 0;
    // first ToC entry whose title (partially) matches name; nullptr if none
    virtual TocItem* FindTocItem(TocItem* item, Str name, bool partially) = 0;
};

enum class PageInfoState {
    Unknown,
    Known,
    Error,
};

extern Kind kindEngineMupdf;
extern Kind kindEngineDjVu;
extern Kind kindEngineImage;
extern Kind kindEngineImageDir;
extern Kind kindEngineComicBooks;
extern Kind kindEnginePostScript;
extern Kind kindEngineEpub;
extern Kind kindEngineFb2;
extern Kind kindEngineMobi;
extern Kind kindEnginePdb;
extern Kind kindEngineChm;
extern Kind kindEngineHtml;
extern Kind kindEngineTxt;

bool IsExternalUrl(Str url);

static inline void SetDefaultExt(Str& ext, Str snew) {
    str::Free(ext);
    ext = str::Dup(snew);
}

/* certain OCGs will only be rendered for some of these (e.g. watermarks) */
enum class RenderTarget {
    View,
    Print,
    Export
};

struct PageLayout {
    enum class Type {
        Single = 0,
        Facing,
        Book,
    };
    PageLayout() = default;
    explicit PageLayout(Type t) { type = t; }
    Type type{Type::Single};
    bool r2l = false;
    // the document stated its reading direction (PDF /Direction or EPUB
    // page-progression-direction), so r2l is its wish rather than our default.
    // A remembered setting must not silently overrule it
    bool r2lDeclared = false;
    bool nonContinuous = false;
};

// PDF page boxes (ISO 32000). MediaBox is required; Crop/Bleed/Trim/Art are
// optional and inherit from the parent /Pages node. They are per-page.
enum class PdfPageBoxKind : u8 {
    Media = 0,
    Crop,
    Bleed,
    Trim,
    Art
};

struct PdfPageBox {
    PdfPageBoxKind kind{};
    RectF rect;
};

const char* PdfPageBoxName(PdfPageBoxKind kind);

extern Kind kindDestinationNone;
extern Kind kindDestinationScrollTo;
extern Kind kindDestinationLaunchURL;
extern Kind kindDestinationLaunchEmbedded;
extern Kind kindDestinationAttachment;
extern Kind kindDestinationLaunchFile;
extern Kind kindDestinationDjVu;
extern Kind kindDestinationMupdf;
extern Kind kindDestinationJsMenu;

enum class TextExtractionState {
    NotExtracted,
    Pending,
    Finished,
};

// text is a UTF-8 byte string, coords has one entry per Unicode codepoint
struct PageText {
    Str text;
    Rect* coords = nullptr;
    int len = 0;         // number of bytes in text, not including the terminating null
    int nCodepoints = 0; // number of Unicode codepoints and bounding boxes in coords
};

void FreePageText(PageText*);

// a link destination
struct IPageDestination : KindBase {
    // page the destination points to (-1 for external destinations such as URLs)
    int pageNo = -1;
    RectF rect;
    float zoom = 0.f;

    IPageDestination() = default;
    virtual ~IPageDestination() = default;

    // rectangle of the destination on the above returned page
    virtual RectF GetRect2() { return rect; }
    // optional zoom level on the above returned page
    virtual float GetZoom2() { return zoom; }
    // anchor point (x, y) on the destination page; rect's dx/dy may be 0.
    // Default falls back to GetRect2 (callers should still tolerate (0,0)).
    virtual RectF GetDestPoint2() { return GetRect2(); }

    // string value associated with the destination (e.g. a path or a URL)
    virtual Str GetValue2() { return {}; }
    // the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
    // (mainly applicable for links of type "LaunchFile" to PDF documents)
    virtual Str GetName2() { return {}; }
};

static inline Str PageDestGetName(IPageDestination* dest) {
    return dest->GetName2();
}

static inline Str PageDestGetValue(IPageDestination* dest) {
    return dest->GetValue2();
}

// true when the destination's value is an address worth copying (a URL or a
// file path). A link inside the document has no address; its value is the
// description the PDF gives it, which is for showing, not for copying
static inline bool PageDestHasAddress(IPageDestination* dest) {
    if (!dest || !dest->GetValue2()) {
        return false;
    }
    Kind k = dest->GetKind();
    return k == kindDestinationLaunchURL || k == kindDestinationLaunchFile;
}

static inline int PageDestGetPageNo(IPageDestination* dest) {
    if (!dest) {
        return -1;
    }
    return dest->pageNo;
}

// rectangle of the destination on the above returned page
static inline RectF PageDestGetRect(IPageDestination* dest) {
    return dest->GetRect2();
}

// anchor point on the destination page (x, y in user-space). Returns {0,0,0,0}
// when the destination has no specific anchor.
static inline RectF PageDestGetDestPoint(IPageDestination* dest) {
    if (!dest) {
        return {};
    }
    return dest->GetDestPoint2();
}

// optional zoom level on the above returned page
static inline float PageDestGetZoom(IPageDestination* dest) {
    return dest->GetZoom2();
}

struct PageDestinationURL : IPageDestination {
    Str url;
    Str displayUrl;

    PageDestinationURL() = delete;

    PageDestinationURL(Str u) {
        ReportIf(!u);
        kind = kindDestinationLaunchURL;
        url = str::Dup(u);
    }

    ~PageDestinationURL() override {
        str::Free(url);
        str::Free(displayUrl);
    }

    Str GetValue2() override {
        if (!url) {
            return {};
        }
        if (!displayUrl) {
            displayUrl = str::Dup(url::DecodeTemp(url));
        }
        return displayUrl;
    }
};

struct PageDestinationFile : IPageDestination {
    Str path;
    Str dest;
    // PDF GoToR /NewWindow (when known). MuPDF's file: URI conversion does not
    // currently preserve this flag; callers may still set it, and Ctrl+click
    // also opens remote files in a new window.
    bool openInNewWindow = false;

    PageDestinationFile() = delete;

    PageDestinationFile(Str u, Str dest) {
        ReportIf(!u);
        kind = kindDestinationLaunchFile;
        path = str::Dup(u);
        this->dest = str::Dup(dest);
    }

    ~PageDestinationFile() override {
        str::Free(path);
        str::Free(dest);
    }

    Str GetValue2() override { return path; }

    Str GetName2() override { return dest; }
};

struct PageDestination : IPageDestination {
    Str value;
    Str name;
    int embedObjNum = 0; // PDF object number for embedded file attachment annotations

    PageDestination() = default;

    ~PageDestination() override;

    Str GetValue2() override;
    Str GetName2() override;
};

// JavaScript app.popUpMenu items (Altium schematic PDFs, issue #1198).
struct PageDestinationJsMenu : IPageDestination {
    StrVec items;
    Str tooltip;

    PageDestinationJsMenu();
    ~PageDestinationJsMenu() override;

    Str GetValue2() override;
};

IPageDestination* NewSimpleDest(int pageNo, RectF rect, float zoom = 0.f, Str value = {});

// use in PageDestination::GetDestRect for values that don't matter
constexpr float kDestUseDefault = -999.9f;

extern Kind kindPageElementDest;
extern Kind kindPageElementImage;
extern Kind kindPageElementComment;

// an element on a page. Might be clicked, provides tooltip info for hoover
struct IPageElement {
    Kind kind = nullptr;
    // position of the element on the page
    RectF rect;
    int pageNo = -1;

    virtual ~IPageElement() = default;

    // the type of this page element
    bool Is(Kind expectedKind);

    Kind GetKind() { return kind; }
    // page this element lives on (-1 for elements in a ToC)
    int GetPageNo() { return pageNo; }

    // position of the element on page, in page coordinates
    RectF GetRect() { return rect; }

    // string value associated with this element (e.g. displayed in an infotip)
    virtual Str GetValue() { return {}; }
    // if this element is a link, this returns information about the link's destination
    // (the result is owned by the PageElement and MUST NOT be deleted)
    virtual IPageDestination* AsLink() { return nullptr; }
    bool IsLink() { return AsLink() != nullptr; }
};

struct PageElementImage : IPageElement {
    int imageID = -1;

    PageElementImage() { kind = kindPageElementImage; }
};

struct PageElementComment : IPageElement {
    Str comment;

    PageElementComment(Str c) {
        kind = kindPageElementComment;
        comment = str::Dup(c);
    }

    ~PageElementComment() override { str::Free(comment); }

    Str GetValue() override { return comment; }
};

struct PageElementDestination : IPageElement {
    IPageDestination* dest;

    PageElementDestination(IPageDestination* d) {
        kind = kindPageElementDest;
        dest = d;
    }

    ~PageElementDestination() override { delete dest; }

    Str GetValue() override {
        if (dest) {
            return dest->GetValue2();
        }
        return {};
    }
    IPageDestination* AsLink() override { return dest; }
};

// those are the same as F font bitmask in PDF docs
// for TocItem::fontFlags
// https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf page 369
constexpr int kFontBitItalic = 0;
constexpr int kFontBitBold = 1;

extern Kind kindTocFzOutline;
extern Kind kindTocFzLink;
extern Kind kindTocFzOutlineAttachment;
extern Kind kindTocDjvu;

// an item in a document's Table of Content
struct TocItem {
    uintptr_t userData = 0;

    TocItem* parent;

    // the item's visible label
    Str title;

    // in some formats, the document can specify the tree item
    // is expanded by default. We keep track if user toggled
    // expansion state of the tree item
    bool isOpenDefault;
    bool isOpenToggled;

    bool isUnchecked;

    // page this item points to (-1 for non-page destinations)
    // if GetLink() returns a destination to a page, the two should match
    int pageNo;

    // arbitrary number allowing to distinguish this TocItem
    // from any other of the same ToC tree (must be constant
    // between runs so that it can be persisted in FileState::tocState)
    int id;

    int fontFlags; // kFontBitBold, kFontBitItalic
    Color color;

    IPageDestination* dest;
    bool destNotOwned;

    // first child item
    TocItem* child;
    // next sibling
    TocItem* next;

    // caching to speed up ChildAt
    TocItem* currChild;
    int currChildNo;

    void AddSibling(TocItem* sibling);
    void AddSiblingAtEnd(TocItem* sibling);
    void AddChild(TocItem* child);

    IPageDestination* GetPageDestination() const;

    int ChildCount();
    TocItem* ChildAt(int n);
    bool IsExpanded();

    bool PageNumbersMatch() const;
};

TocItem* AllocTocItem(Arena* arena, Str title, int pageNo);
void FreeTocItemRec(Arena* arena, TocItem* item);

struct TocTree : TreeModel {
    TocItem* root = nullptr;

    TocTree() = default;
    explicit TocTree(TocItem* root);
    ~TocTree() override;

    TreeItem Root() override;

    Str Text(TreeItem) override;
    TreeItem Parent(TreeItem) override;
    int ChildCount(TreeItem) override;
    TreeItem ChildAt(TreeItem, int idx) override;
    bool IsExpanded(TreeItem) override;
    bool IsChecked(TreeItem) override;

    void SetUserData(TreeItem, uintptr_t) override;
    uintptr_t GetUserData(TreeItem) override;
};

struct VisitTocTreeData {
    TocItem* ti = nullptr;
    TocItem* parent = nullptr; // only for VisitTocTreeWithParent
    bool stopTraversal = false;
};

using VisitTocTreeCb = Func1<VisitTocTreeData*>;

// a helper that allows for rendering interruptions in an engine-agnostic way
class AbortCookie {
  public:
    virtual ~AbortCookie() = default;
    // aborts a rendering request (as far as possible)
    // note: must be thread-safe
    virtual void Abort() = 0;
    virtual void* GetData() = 0;
};

struct DarkModeProfile;

struct RenderPageArgs {
    int pageNo = 0;
    float zoom = 0.f;
    int rotation = 0;
    /* if nullptr: defaults to the page's mediabox */
    RectF* pageRect = nullptr;
    RenderTarget target = RenderTarget::View;
    // the caller paints a background first and composites the page over it, so
    // a page with transparency should keep its alpha instead of being flattened
    // onto white. Only the canvas does that; print and export need an opaque
    // page, and so does anything that blits the result with SRCCOPY (#5844)
    bool keepAlpha = false;
    // clear the page pixmap to alpha 0 so the canvas background (solid colour
    // or a checkerboard) shows through unpainted areas (issue #1809)
    bool transparentBackdrop = false;
    AbortCookie** cookie_out = nullptr;
    // dark/recolor rendering profile for View renders (see PdfDarkMode.h);
    // owned by the caller, only valid for the duration of RenderPage()
    const DarkModeProfile* darkProfile = nullptr;

    RenderPageArgs(int pageNo, float zoom, int rotation, RectF* pageRect = nullptr,
                   RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr);
};

class EngineBase {
  public:
    Kind kind = nullptr;

    Arena* arena = nullptr;
    AtomicRefCount refCount = 1; // starts life as acquired
    // the default file extension for a document like
    // the currently loaded one (e.g. L".pdf")
    Str defaultExt;
    PageLayout preferredLayout;
    float fileDPI = 96.0f;
    bool isImageCollection = false;
    // a document laid out into pages we choose (epub, mobi, fb2, html...),
    // as opposed to one with fixed pages. the ebook settings only apply to
    // these, so the UI uses it to decide what to offer (#4600)
    bool isReflowable = false;
    bool allowsPrinting = true;
    bool allowsCopyingText = true;
    bool isPasswordProtected = false;
    // hex-encoded password fingerprint + crypt key; arena-allocated
    Str decryptionKey;
    bool hasPageLabels = false;
    bool hideAnnotations = false;
    bool disableAntiAlias = false;
    bool disableAutoLinks = false;
    int pageCount = -1;

    // TODO: migrate other engines to use this
    Str fileNameBase;

    EngineBase();

    // creates a clone of this engine (e.g. for printing on a different thread)
    virtual EngineBase* Clone() = 0;

    int AddRef();
    bool Release();

    void AppendError(Str msg);
    bool HasErrors();
    TempStr GetErrorsTextTemp();

    int PageCount() const;

    // the box containing the visible page content (usually RectF(0, 0, pageWidth, pageHeight))
    virtual RectF PageMediabox(int pageNo) = 0;
    virtual RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View);
    virtual void GetPdfPageBoxes(int pageNo, Vec<PdfPageBox>& out);

    // renders a page into a cacheable Pixmap
    // (*cookie_out must be deleted after the call returns)
    virtual Pixmap* RenderPage(RenderPageArgs& args) = 0;

    PointF Transform(PointF pt, int pageNo, float zoom, int rotation, bool inverse = false);
    virtual RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) = 0;

    // returns the binary data for the current file
    // (e.g. for saving again when the file has already been deleted)
    // caller needs to free() the result
    virtual Str GetFileData() = 0;

    // saves a copy of the current file under a different name (overwriting an existing file)
    virtual bool SaveFileAs(Str dstPath) = 0;

    // extracts all text found in the given page (and optionally also the
    // coordinates of the individual glyphs)
    // caller needs to free() the result and *coordsOut (if coordsOut is non-nullptr)
    virtual PageText ExtractPageText(int) { return {}; }
    virtual bool TryExtractPageText(int pageNo, PageText* out);

    bool HasTextForPage(int pageNo);
    TextExtractionState GetTextExtractionState(int pageNo);
    void RequestTextExtraction(int pageNo);
    Str GetTextForPage(int pageNo, int* lenOut = nullptr, Rect** coordsOut = nullptr);
    bool TryGetTextForPage(int pageNo, int* lenOut = nullptr, Rect** coordsOut = nullptr);
    void InvalidateTextForPage(int pageNo);
    virtual void ReleaseTextExtractionThreadContext() {}
    // pages where clipping doesn't help are rendered in larger tiles
    virtual bool HasClipOptimizations(int pageNo) = 0;

    bool IsImageCollection() const;

    // access to various document properties (such as Author, Title, etc.)
    virtual TempStr GetPropertyTemp(DocProp prop) = 0;

    virtual void GetProperties(Vec<PropValue>& propsOut);

    virtual bool AllowsPrinting() const;

    bool AllowsCopyingText() const;

    float GetFileDPI() const;

    // returns a list of all available elements for this page
    // caller must delete the Vec but not the elements inside the vector
    virtual Vec<IPageElement*> GetElements(int pageNo) = 0;
    virtual bool TryGetElements(int pageNo, Vec<IPageElement*>* out);

    // returns the element at a given point or nullptr if there's none
    virtual IPageElement* GetElementAtPos(int pageNo, PointF pt) = 0;

    virtual IPageDestination* GetNamedDest(Str name);

    // 1-based page from safe PDF /OpenAction GoTo, or 0 (issue #1631)
    virtual int GetOpenActionPageNo() { return 0; }

    virtual bool HasToc();

    virtual TocTree* GetToc();

    bool HasPageLabels() const;

    virtual TempStr GetPageLabeTemp(int pageNo) const;

    virtual int GetPageByLabel(Str label) const;

    bool IsPasswordProtected() const;

    // loads the given page so that the time required can be measured
    // without also measuring rendering times
    virtual bool BenchLoadPage(int pageNo) = 0;

    Str FilePath() const;

    virtual RenderedBitmap* GetImageForPageElement(IPageElement*);
    virtual Str GetImageDataForPageElement(IPageElement*);

    virtual bool HandleLink(IPageDestination*, ILinkHandler*);

    // bitmap regions (in device pixels of the rendered tile) whose original
    // colors should be preserved by the dark-mode bitmap recolor pass
    // (photos / artwork); default: none
    virtual void GetBitmapRecolorSkipRects(int pageNo, float zoom, int rotation, const RectF& renderPageRect,
                                           Size bmpSize, Vec<Rect>& skipRects) {
        (void)pageNo;
        (void)zoom;
        (void)rotation;
        (void)renderPageRect;
        (void)bmpSize;
        VecClear(skipRects);
    }

    void SetFilePath(Str s);

  protected:
    virtual ~EngineBase();

    // cached text, one entry per page (lazily allocated)
    PageText* pagesText = nullptr;
    TextExtractionState* pagesTextState = nullptr;
    Mutex textCacheLock;

    str::Builder errors;
    Mutex errorsLock;
};

struct PasswordUI {
    virtual Str GetPassword(Str path, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) = 0;
    virtual ~PasswordUI() = default;
};

enum class PdfDuplexPref {
    Simplex,
    FlipShortEdge,
    FlipLongEdge
};

// print-related entries read from a PDF's /ViewerPreferences dictionary
// (issue #534). Each value has a matching "has" flag because the keys are
// all optional.
struct PdfViewerPrintPrefs {
    bool hasPickTrayByPdfSize = false;
    bool pickTrayByPdfSize = false;
    bool hasNumCopies = false;
    int numCopies = 0;
    bool hasDuplex = false;
    PdfDuplexPref duplex = PdfDuplexPref::Simplex;
    bool hasPrintScaling = false;
    bool printScalingNone = false; // true when /PrintScaling is /None
};

bool GetPdfViewerPrintPrefs(EngineBase* engine, PdfViewerPrintPrefs& prefs);
bool SaveFileOrData(Str srcFilePath, Str data, Str dstFilePath);

template <typename T>
void SafeEngineRelease(T** enginePtr) {
    T* engine = *enginePtr;
    if (engine) {
        engine->Release();
        *enginePtr = nullptr;
    }
}
