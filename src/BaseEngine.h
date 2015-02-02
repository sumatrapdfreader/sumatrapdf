/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* certain OCGs will only be rendered for some of these (e.g. watermarks) */
enum RenderTarget { Target_View, Target_Print, Target_Export };

enum PageLayoutType { Layout_Single = 0, Layout_Facing = 1, Layout_Book = 2,
                      Layout_R2L = 16, Layout_NonContinuous = 32 };

enum PageElementType { Element_Link, Element_Image, Element_Comment };

enum PageDestType { Dest_None,
    Dest_ScrollTo, Dest_LaunchURL, Dest_LaunchEmbedded, Dest_LaunchFile,
    Dest_NextPage, Dest_PrevPage, Dest_FirstPage, Dest_LastPage,
    Dest_FindDialog, Dest_FullScreen, Dest_GoBack, Dest_GoForward,
    Dest_GoToPageDialog, Dest_PrintDialog, Dest_SaveAsDialog, Dest_ZoomToDialog,
};

enum PageAnnotType {
    Annot_None,
    Annot_Highlight, Annot_Underline, Annot_StrikeOut, Annot_Squiggly,
};

enum DocumentProperty {
    Prop_Title, Prop_Author, Prop_Copyright, Prop_Subject,
    Prop_CreationDate, Prop_ModificationDate, Prop_CreatorApp,
    Prop_UnsupportedFeatures, Prop_FontList,
    Prop_PdfVersion, Prop_PdfProducer, Prop_PdfFileStructure,
};

class RenderedBitmap {
protected:
    HBITMAP hbmp;
    SizeI size;
    ScopedHandle hMap;

public:
    RenderedBitmap(HBITMAP hbmp, SizeI size, HANDLE hMap=nullptr) : hbmp(hbmp), size(size), hMap(hMap) { }
    ~RenderedBitmap() { DeleteObject(hbmp); }

    RenderedBitmap *Clone() const {
        HBITMAP hbmp2 = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, size.dx, size.dy, 0);
        return new RenderedBitmap(hbmp2, size);
    }

    // callers must not delete this (use Clone if you have to modify it)
    HBITMAP GetBitmap() const { return hbmp; }
    SizeI Size() const { return size; }

    // render the bitmap into the target rectangle (streching and skewing as requird)
    bool StretchDIBits(HDC hdc, RectI target) const {
        HDC bmpDC = CreateCompatibleDC(hdc);
        if (!bmpDC)
            return false;
        HGDIOBJ oldBmp = SelectObject(bmpDC, hbmp);
        if (!oldBmp) {
            DeleteDC(bmpDC);
            return false;
        }
        SetStretchBltMode(hdc, HALFTONE);
        bool ok = StretchBlt(hdc, target.x, target.y, target.dx, target.dy,
                             bmpDC, 0, 0, size.dx, size.dy, SRCCOPY);
        SelectObject(bmpDC, oldBmp);
        DeleteDC(bmpDC);
        return ok;
    }
};

// interface to be implemented for saving embedded documents that a link points to
class LinkSaverUI {
public:
    virtual bool SaveEmbedded(const unsigned char *data, size_t cbCount) = 0;
    virtual ~LinkSaverUI() { }
};

// a link destination
class PageDestination {
public:
    virtual ~PageDestination() { }
    // type of the destination (most common are Dest_ScrollTo and Dest_LaunchURL)
    virtual PageDestType GetDestType() const = 0;
    // page the destination points to (0 for external destinations such as URLs)
    virtual int GetDestPageNo() const = 0;
    // rectangle of the destination on the above returned page
    virtual RectD GetDestRect() const = 0;
    // string value associated with the destination (e.g. a path or a URL)
    // caller must free() the result
    virtual WCHAR *GetDestValue() const { return nullptr; }
    // the name of this destination (reverses BaseEngine::GetNamedDest) or nullptr
    // (mainly applicable for links of type "LaunchFile" to PDF documents)
    // caller must free() the result
    virtual WCHAR *GetDestName() const { return nullptr; }

    // if this destination's target is an embedded file, this allows to
    // save that file efficiently (the LinkSaverUI might get passed a link
    // to an internal buffer in order to avoid unnecessary memory allocations)
    virtual bool SaveEmbedded(LinkSaverUI &saveUI) { return false; }
};

// an user annotation on page
struct PageAnnotation {
    struct Color {
        uint8_t r, g, b, a;
        Color() : r(0), g(0), b(0), a(0) { }
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255) : r(r), g(g), b(b), a(a) { }
        explicit Color(COLORREF c, uint8_t a=255) :
            r(GetRValueSafe(c)), g(GetGValueSafe(c)), b(GetBValueSafe(c)), a(a) { }
        bool operator==(const Color& other) const {
            return other.r == r && other.g == g && other.b == b && other.a == a;
        }
    };

    PageAnnotType type;
    int pageNo;
    RectD rect;
    Color color;

    PageAnnotation() : type(Annot_None), pageNo(-1) { }
    PageAnnotation(PageAnnotType type, int pageNo, RectD rect, Color color) :
        type(type), pageNo(pageNo), rect(rect), color(color) { }
    bool operator==(const PageAnnotation& other) const {
        return other.type == type && other.pageNo == pageNo &&
               other.rect == rect && other.color == color;
    }
};

// use in PageDestination::GetDestRect for values that don't matter
#define DEST_USE_DEFAULT    -999.9

// hoverable (and maybe interactable) element on a single page
class PageElement {
public:
    virtual ~PageElement() { }
    // the type of this page element
    virtual PageElementType GetType() const = 0;
    // page this element lives on (0 for elements in a ToC)
    virtual int GetPageNo() const = 0;
    // rectangle that can be interacted with
    virtual RectD GetRect() const = 0;
    // string value associated with this element (e.g. displayed in an infotip)
    // caller must free() the result
    virtual WCHAR *GetValue() const = 0;

    // if this element is a link, this returns information about the link's destination
    // (the result is owned by the PageElement and MUST NOT be deleted)
    virtual PageDestination *AsLink() { return nullptr; }
    // if this element is an image, this returns it
    // caller must delete the result
    virtual RenderedBitmap *GetImage() { return nullptr; }
};

// an item in a document's Table of Content
class DocTocItem {
    DocTocItem *last; // only updated by AddSibling

public:
    // the item's visible label
    WCHAR *title;
    // whether any child elements are to be displayed
    bool open;
    // page this item points to (0 for non-page destinations)
    // if GetLink() returns a destination to a page, the two should match
    int pageNo;
    // arbitrary number allowing to distinguish this DocTocItem
    // from any other of the same ToC tree (must be constant
    // between runs so that it can be persisted in FileState::tocState)
    int id;

    // first child item
    DocTocItem *child;
    // next sibling
    DocTocItem *next;

    explicit DocTocItem(WCHAR *title, int pageNo=0) :
        title(title), open(false), pageNo(pageNo), id(0), child(nullptr), next(nullptr), last(nullptr) { }

    virtual ~DocTocItem() {
        delete child;
        while (next) {
            DocTocItem *tmp = next->next;
            next->next = nullptr;
            delete next;
            next = tmp;
        }
        free(title);
    }

    void AddSibling(DocTocItem *sibling) {
        DocTocItem *item;
        for (item = last ? last : this; item->next; item = item->next);
        last = item->next = sibling;
    }

    void OpenSingleNode() {
        // only open (root level) ToC nodes if there's at most two
        if (!next || !next->next) {
            open = true;
            if (next)
                next->open = true;
        }
    }

    // returns the destination this ToC item points to or nullptr
    // (the result is owned by the DocTocItem and MUST NOT be deleted)
    virtual PageDestination *GetLink() = 0;
};

// a helper that allows for rendering interruptions in an engine-agnostic way
class AbortCookie {
public:
    virtual ~AbortCookie() { }
    // aborts a rendering request (as far as possible)
    // note: must be thread-safe
    virtual void Abort() = 0;
};

class BaseEngine {
public:
    virtual ~BaseEngine() { }
    // creates a clone of this engine (e.g. for printing on a different thread)
    virtual BaseEngine *Clone() = 0;

    // the name of the file this engine handles
    virtual const WCHAR *FileName() const = 0;
    // number of pages the loaded document contains
    virtual int PageCount() const = 0;

    // the box containing the visible page content (usually RectD(0, 0, pageWidth, pageHeight))
    virtual RectD PageMediabox(int pageNo) = 0;
    // the box inside PageMediabox that actually contains any relevant content
    // (used for auto-cropping in Fit Content mode, can be PageMediabox)
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        return PageMediabox(pageNo);
    }

    // renders a page into a cacheable RenderedBitmap
    // (*cookie_out must be deleted after the call returns)
    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=nullptr, /* if nullptr: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=nullptr) = 0;

    // applies zoom and rotation to a point in user/page space converting
    // it into device/screen space - or in the inverse direction
    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false) = 0;
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false) = 0;

    // returns the binary data for the current file
    // (e.g. for saving again when the file has already been deleted)
    // caller needs to free() the result
    virtual unsigned char *GetFileData(size_t *cbCount) = 0;
    // saves a copy of the current file under a different name (overwriting an existing file)
    // (includeUserAnnots only has an effect if SupportsAnnotation(true) returns true)
    virtual bool SaveFileAs(const WCHAR *copyFileName, bool includeUserAnnots=false) = 0;
    // converts the current file to a PDF file and saves it (overwriting an existing file),
    // (includeUserAnnots should always have an effect)
    virtual bool SaveFileAsPDF(const WCHAR *pdfFileName, bool includeUserAnnots=false) { return false; }
    // extracts all text found in the given page (and optionally also the
    // coordinates of the individual glyphs)
    // caller needs to free() the result and *coords_out (if coords_out is non-nullptr)
    virtual WCHAR * ExtractPageText(int pageNo, const WCHAR *lineSep, RectI **coords_out=nullptr,
                                    RenderTarget target=Target_View) = 0;
    // pages where clipping doesn't help are rendered in larger tiles
    virtual bool HasClipOptimizations(int pageNo) = 0;
    // the layout type this document's author suggests (if the user doesn't care)
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }
    // whether the content should be displayed as images instead of as document pages
    // (e.g. with a black background and less padding in between and without search UI)
    virtual bool IsImageCollection() const { return false; }

    // access to various document properties (such as Author, Title, etc.)
    virtual WCHAR *GetProperty(DocumentProperty prop) = 0;

    // TODO: generalize from PageAnnotation to PageModification
    // whether this engine supports adding user annotations of all available types
    // (either for rendering or for saving)
    virtual bool SupportsAnnotation(bool forSaving=false) const = 0;
    // informs the engine about annotations the user made so that they can be rendered, etc.
    // (this call supercedes any prior call to UpdateUserAnnotations)
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list) = 0;

    // TODO: needs a more general interface
    // whether it is allowed to print the current document
    virtual bool AllowsPrinting() const { return true; }
    // whether it is allowed to extract text from the current document
    // (except for searching an accessibility reasons)
    virtual bool AllowsCopyingText() const { return true; }

    // the DPI for a file is needed when converting internal measures to physical ones
    virtual float GetFileDPI() const { return 96.0f; }
    // the default file extension for a document like the currently loaded one (e.g. L".pdf")
    virtual const WCHAR *GetDefaultFileExt() const = 0;

    // returns a list of all available elements for this page
    // caller must delete the result (including all elements contained in the Vec)
    virtual Vec<PageElement *> *GetElements(int pageNo) = 0;
    // returns the element at a given point or nullptr if there's none
    // caller must delete the result
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt) = 0;

    // creates a PageDestination from a name (or nullptr for invalid names)
    // caller must delete the result
    virtual PageDestination *GetNamedDest(const WCHAR *name) { return nullptr; }
    // checks whether this document has an associated Table of Contents
    virtual bool HasTocTree() const { return false; }
    // returns the root element for the loaded document's Table of Contents
    // caller must delete the result (when no longer needed)
    virtual DocTocItem *GetTocTree() { return nullptr; }

    // checks whether this document has explicit labels for pages (such as
    // roman numerals) instead of the default plain arabic numbering
    virtual bool HasPageLabels() const { return false; }
    // returns a label to be displayed instead of the page number
    // caller must free() the result
    virtual WCHAR *GetPageLabel(int pageNo) const { return str::Format(L"%d", pageNo); }
    // reverts GetPageLabel by returning the first page number having the given label
    virtual int GetPageByLabel(const WCHAR *label) const { return _wtoi(label); }

    // whether this document required a password in order to be loaded
    virtual bool IsPasswordProtected() const { return false; }
    // returns a string to remember when the user wants to save a document's password
    // (don't implement for document types that don't support password protection)
    // caller must free() the result
    virtual char *GetDecryptionKey() const { return nullptr; }

    // loads the given page so that the time required can be measured
    // without also measuring rendering times
    virtual bool BenchLoadPage(int pageNo) = 0;
};

class PasswordUI {
public:
    virtual WCHAR * GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) = 0;
    virtual ~PasswordUI() { }
};
