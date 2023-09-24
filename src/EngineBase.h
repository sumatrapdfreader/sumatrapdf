/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct fz_outline;
struct fz_link;

extern Kind kindEngineMupdf;
extern Kind kindEngineMulti;
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

bool IsExternalUrl(const WCHAR* url);
bool IsExternalUrl(const char* url);

/* certain OCGs will only be rendered for some of these (e.g. watermarks) */
enum class RenderTarget { View, Print, Export };

struct PageLayout {
    enum class Type {
        Single = 0,
        Facing,
        Book,
    };
    PageLayout() = default;
    explicit PageLayout(Type t) {
        type = t;
    }
    Type type{Type::Single};
    bool r2l = false;
    bool nonContinuous = false;
};

extern Kind kindDestinationNone;
extern Kind kindDestinationScrollTo;
extern Kind kindDestinationLaunchURL;
extern Kind kindDestinationLaunchEmbedded;
extern Kind kindDestinationAttachment;
extern Kind kindDestinationLaunchFile;
extern Kind kindDestinationDjVu;
extern Kind kindDestinationMupdf;

// text on a page
// a character and its bounding box in page coordinates
struct PageText {
    WCHAR* text = nullptr;
    Rect* coords = nullptr;
    int len = 0; // number of chars in text and bounding boxes in coords
};

void FreePageText(PageText*);

// a link destination
struct IPageDestination {
    Kind kind = nullptr;

    int pageNo = -1;
    RectF rect = {};
    float zoom = 0.f;

    IPageDestination() = default;
    virtual ~IPageDestination(){};

    Kind GetKind() {
        return kind;
    }

    // page the destination points to (-1 for external destinations such as URLs)
    virtual int GetPageNo() {
        return pageNo;
    }
    // rectangle of the destination on the above returned page
    virtual RectF GetRect() {
        return rect;
    }
    // optional zoom level on the above returned page
    virtual float GetZoom() {
        return zoom;
    }

    // string value associated with the destination (e.g. a path or a URL)
    virtual char* GetValue() {
        return nullptr;
    }
    // the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
    // (mainly applicable for links of type "LaunchFile" to PDF documents)
    virtual char* GetName() {
        return nullptr;
    }
};

struct PageDestinationURL : IPageDestination {
    char* url = nullptr;

    PageDestinationURL() = delete;

    PageDestinationURL(const char* u) {
        CrashIf(!u);
        kind = kindDestinationLaunchURL;
        url = str::Dup(u);
    }

    ~PageDestinationURL() override {
        str::Free(url);
    }

    char* GetValue() override {
        return url;
    }
};

struct PageDestinationFile : IPageDestination {
    char* path = nullptr;
    char* name = nullptr;

    PageDestinationFile() = delete;

    PageDestinationFile(const char* u, const char* frag) {
        CrashIf(!u);
        kind = kindDestinationLaunchFile;
        path = str::Dup(u);
        name = str::Dup(frag);
    }

    ~PageDestinationFile() override {
        str::Free(path);
        str::Free(name);
    }

    char* GetValue() override {
        return path;
    }

    char* GetName() override {
        return name;
    }
};

struct PageDestination : IPageDestination {
    char* value = nullptr;
    char* name = nullptr;

    PageDestination() = default;

    ~PageDestination() override;

    char* GetValue() override;
    char* GetName() override;
};

IPageDestination* NewSimpleDest(int pageNo, RectF rect, float zoom = 0.f, const char* value = nullptr);

// use in PageDestination::GetDestRect for values that don't matter
#define DEST_USE_DEFAULT -999.9f

extern Kind kindPageElementDest;
extern Kind kindPageElementImage;
extern Kind kindPageElementComment;

// an element on a page. Might be clicked, provides tooltip info for hoover
struct IPageElement {
    Kind kind = nullptr;
    // position of the element on the page
    RectF rect{};
    int pageNo = -1;

    virtual ~IPageElement() = default;

    // the type of this page element
    bool Is(Kind expectedKind);

    Kind GetKind() {
        return kind;
    }
    // page this element lives on (-1 for elements in a ToC)
    int GetPageNo() {
        return pageNo;
    }

    // position of the element on page, in page coordinates
    RectF GetRect() {
        return rect;
    }

    // string value associated with this element (e.g. displayed in an infotip)
    // caller must free() the result
    virtual char* GetValue() {
        return nullptr;
    }
    // if this element is a link, this returns information about the link's destination
    // (the result is owned by the PageElement and MUST NOT be deleted)
    virtual IPageDestination* AsLink() {
        return nullptr;
    }
    bool IsLink() {
        return AsLink() != nullptr;
    }
};

struct PageElementImage : IPageElement {
    int imageID = -1;

    PageElementImage() {
        kind = kindPageElementImage;
    }
};

struct PageElementComment : IPageElement {
    char* comment = nullptr;

    PageElementComment(const char* c) {
        kind = kindPageElementComment;
        comment = str::Dup(c);
    }

    ~PageElementComment() override {
        str::Free(comment);
    }

    char* GetValue() override {
        return comment;
    }
};

struct PageElementDestination : IPageElement {
    IPageDestination* dest;

    PageElementDestination(IPageDestination* d) {
        kind = kindPageElementDest;
        dest = d;
    }

    ~PageElementDestination() override {
        delete dest;
    }

    char* GetValue() override {
        if (dest) {
            return dest->GetValue();
        }
        return nullptr;
    }
    IPageDestination* AsLink() override {
        return dest;
    }
};

// those are the same as F font bitmask in PDF docs
// for TocItem::fontFlags
// https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf page 369
constexpr int fontBitItalic = 0;
constexpr int fontBitBold = 1;

extern Kind kindTocFzOutline;
extern Kind kindTocFzLink;
extern Kind kindTocFzOutlineAttachment;
extern Kind kindTocDjvu;

// an item in a document's Table of Content
struct TocItem {
    HTREEITEM hItem = nullptr;

    TocItem* parent = nullptr;

    // the item's visible label
    char* title = nullptr;

    // in some formats, the document can specify the tree item
    // is expanded by default. We keep track if user toggled
    // expansion state of the tree item
    bool isOpenDefault = false;
    bool isOpenToggled = false;

    bool isUnchecked = false;

    // page this item points to (-1 for non-page destinations)
    // if GetLink() returns a destination to a page, the two should match
    int pageNo = 0;

    // arbitrary number allowing to distinguish this TocItem
    // from any other of the same ToC tree (must be constant
    // between runs so that it can be persisted in FileState::tocState)
    int id = 0;

    int fontFlags = 0; // fontBitBold, fontBitItalic
    COLORREF color{ColorUnset};

    IPageDestination* dest = nullptr;
    bool destNotOwned = false;

    // first child item
    TocItem* child = nullptr;
    // next sibling
    TocItem* next = nullptr;

    // caching to speed up ChildAt
    TocItem* currChild = nullptr;
    int currChildNo = 0;

    // -- only for .EngineMulti
    // marks a node that represents a file
    char* engineFilePath = nullptr;
    int nPages = 0;
    // auto-calculated page number that tells us a span from
    // pageNo => endPageNo
    int endPageNo = 0;

    TocItem() = default;

    explicit TocItem(TocItem* parent, const char* title, int pageNo);

    ~TocItem();

    void AddSibling(TocItem* sibling);
    void AddSiblingAtEnd(TocItem* sibling);
    void AddChild(TocItem* child);

    void DeleteJustSelf();

    IPageDestination* GetPageDestination() const;

    int ChildCount();
    TocItem* ChildAt(int n);
    bool IsExpanded();

    bool PageNumbersMatch() const;
};

struct TocTree : TreeModel {
    TocItem* root = nullptr;

    TocTree() = default;
    explicit TocTree(TocItem* root);
    ~TocTree() override;

    // TreeModel
    TreeItem Root() override;

    char* Text(TreeItem) override;
    TreeItem Parent(TreeItem) override;
    int ChildCount(TreeItem) override;
    TreeItem ChildAt(TreeItem, int index) override;
    bool IsExpanded(TreeItem) override;
    bool IsChecked(TreeItem) override;

    void SetHandle(TreeItem, HTREEITEM) override;
    HTREEITEM GetHandle(TreeItem) override;
};

bool VisitTocTree(TocItem* ti, const std::function<bool(TocItem*)>& f);
bool VisitTocTreeWithParent(TocItem* ti, const std::function<bool(TocItem* ti, TocItem* parent)>& f);
void SetTocTreeParents(TocItem* treeRoot);

// a helper that allows for rendering interruptions in an engine-agnostic way
class AbortCookie {
  public:
    virtual ~AbortCookie() = default;
    // aborts a rendering request (as far as possible)
    // note: must be thread-safe
    virtual void Abort() = 0;
};

struct RenderPageArgs {
    int pageNo = 0;
    float zoom = 0.f;
    int rotation = 0;
    /* if nullptr: defaults to the page's mediabox */
    RectF* pageRect = nullptr;
    RenderTarget target = RenderTarget::View;
    AbortCookie** cookie_out = nullptr;

    RenderPageArgs(int pageNo, float zoom, int rotation, RectF* pageRect = nullptr,
                   RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr);
};

class EngineBase {
  public:
    Kind kind = nullptr;
    // the default file extension for a document like
    // the currently loaded one (e.g. L".pdf")
    const char* defaultExt = nullptr;
    PageLayout preferredLayout;
    float fileDPI = 96.0f;
    bool isImageCollection = false;
    bool allowsPrinting = true;
    bool allowsCopyingText = true;
    bool isPasswordProtected = false;
    char* decryptionKey = nullptr;
    bool hasPageLabels = false;
    int pageCount = -1;

    // TODO: migrate other engines to use this
    AutoFreeStr fileNameBase;

    virtual ~EngineBase();
    // creates a clone of this engine (e.g. for printing on a different thread)
    virtual EngineBase* Clone() = 0;

    // number of pages the loaded document contains
    int PageCount() const;

    // the box containing the visible page content (usually RectF(0, 0, pageWidth, pageHeight))
    virtual RectF PageMediabox(int pageNo) = 0;
    // the box inside PageMediabox that actually contains any relevant content
    // (used for auto-cropping in Fit Content mode, can be PageMediabox)
    virtual RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View);

    // renders a page into a cacheable RenderedBitmap
    // (*cookie_out must be deleted after the call returns)
    virtual RenderedBitmap* RenderPage(RenderPageArgs& args) = 0;

    // applies zoom and rotation to a point in user/page space converting
    // it into device/screen space - or in the inverse direction
    PointF Transform(PointF pt, int pageNo, float zoom, int rotation, bool inverse = false);
    virtual RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) = 0;

    // returns the binary data for the current file
    // (e.g. for saving again when the file has already been deleted)
    // caller needs to free() the result
    virtual ByteSlice GetFileData() = 0;

    // saves a copy of the current file under a different name (overwriting an existing file)
    virtual bool SaveFileAs(const char* copyFileName) = 0;

    // extracts all text found in the given page (and optionally also the
    // coordinates of the individual glyphs)
    // caller needs to free() the result and *coordsOut (if coordsOut is non-nullptr)
    virtual PageText ExtractPageText(int pageNo) = 0;
    // pages where clipping doesn't help are rendered in larger tiles
    virtual bool HasClipOptimizations(int pageNo) = 0;

    // the layout type this document's author suggests (if the user doesn't care)
    // whether the content should be displayed as images instead of as document pages
    // (e.g. with a black background and less padding in between and without search UI)
    bool IsImageCollection() const;

    // access to various document properties (such as Author, Title, etc.)
    virtual char* GetProperty(DocumentProperty prop) = 0;

    // TODO: needs a more general interface
    // whether it is allowed to print the current document
    virtual bool AllowsPrinting() const;

    // whether it is allowed to extract text from the current document
    // (except for searching an accessibility reasons)
    bool AllowsCopyingText() const;

    // the DPI for a file is needed when converting internal measures to physical ones
    float GetFileDPI() const;

    // returns a list of all available elements for this page
    // caller must delete the Vec but not the elements inside the vector
    virtual Vec<IPageElement*> GetElements(int pageNo) = 0;

    // returns the element at a given point or nullptr if there's none
    virtual IPageElement* GetElementAtPos(int pageNo, PointF pt) = 0;

    // creates a PageDestination from a name (or nullptr for invalid names)
    // caller must delete the result
    virtual IPageDestination* GetNamedDest(const char* name);

    // checks whether this document has an associated Table of Contents
    bool HasToc();

    // returns the root element for the loaded document's Table of Contents
    // caller must delete the result (when no longer needed)
    virtual TocTree* GetToc();

    // checks whether this document has explicit labels for pages (such as
    // roman numerals) instead of the default plain arabic numbering
    bool HasPageLabels() const;

    // returns a label to be displayed instead of the page number
    // caller must free() the result
    virtual char* GetPageLabel(int pageNo) const;

    // reverts GetPageLabel by returning the first page number having the given label
    virtual int GetPageByLabel(const char* label) const;

    // whether this document required a password in order to be loaded
    bool IsPasswordProtected() const;

    // returns a string to remember when the user wants to save a document's password
    // (don't implement for document types that don't support password protection)
    // caller must free() the result
    char* GetDecryptionKey() const;

    // loads the given page so that the time required can be measured
    // without also measuring rendering times
    virtual bool BenchLoadPage(int pageNo) = 0;

    // the name of the file this engine handles
    const char* FilePath() const;

    virtual RenderedBitmap* GetImageForPageElement(IPageElement*);

    // returns false if didn't perform action (temporary until we move
    // all code there)
    virtual bool HandleLink(IPageDestination*, ILinkHandler*);

    // protected:
    void SetFilePath(const char* s);
};

struct PasswordUI {
    virtual char* GetPassword(const char* fileName, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) = 0;
    virtual ~PasswordUI() = default;
};

TempStr CleanupFileURLTemp(const char* s);

TempStr CleanupURLForClipbardCopyTemp(const char* s);
