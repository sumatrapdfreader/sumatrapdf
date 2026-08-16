
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "TreeModel.h"

#include "EngineBase.h"

Kind kindPageElementDest = "dest";
Kind kindPageElementImage = "image";
Kind kindPageElementComment = "comment";

Kind kindDestinationNone = "none";
Kind kindDestinationScrollTo = "scrollTo";
Kind kindDestinationLaunchURL = "launchURL";
Kind kindDestinationLaunchEmbedded = "launchEmbedded";
Kind kindDestinationAttachment = "launchAttachment";
Kind kindDestinationLaunchFile = "launchFile";
Kind kindDestinationDjVu = "destinationDjVu";
Kind kindDestinationMupdf = "destinationMupdf";

// clang-format off
static Kind destKinds[] = {
    kindDestinationNone,
    kindDestinationScrollTo,
    kindDestinationLaunchURL,
    kindDestinationLaunchEmbedded,
    kindDestinationAttachment,
    kindDestinationLaunchFile,
    kindDestinationDjVu,
    kindDestinationMupdf
};
// clang-format on

bool IsExternalUrl(Str url) {
    return str::StartsWithI(url, StrL("http://")) || str::StartsWithI(url, StrL("https://")) ||
           str::StartsWithI(url, StrL("mailto:"));
}

static void EnsurePageText(PageText* pageText) {
    if (pageText->text) {
        if (pageText->len == 0) {
            pageText->len = pageText->text.len;
        }
        if (pageText->nCodepoints == 0) {
            pageText->nCodepoints = Utf8CodepointCount(pageText->text);
        }
        return;
    }
    // TakeStr()/Vec::Take() can allocate backing storage even for empty pages.
    str::Free(pageText->text);
    free((void*)pageText->coords);
    pageText->text = {};
    pageText->coords = nullptr;
    pageText->len = 0;
    pageText->nCodepoints = 0;
}

void FreePageText(PageText* pageText) {
    str::Free(pageText->text);
    free((void*)pageText->coords);
    pageText->text = {};
    pageText->coords = nullptr;
    pageText->len = 0;
    pageText->nCodepoints = 0;
}

PageDestination::~PageDestination() {
    str::Free(value);
    str::Free(name);
}

// string value associated with the destination (e.g. a path or a URL)
Str PageDestination::GetValue2() {
    return value;
}

// the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
// (mainly applicable for links of type "LaunchFile" to PDF documents)
Str PageDestination::GetName2() {
    return name;
}

IPageDestination* NewSimpleDest(int pageNo, RectF rect, float zoom, Str value) {
    if (value) {
        return new PageDestinationURL(value);
    }
    auto* res = new PageDestination();
    res->pageNo = pageNo;
    res->rect = rect;
    res->kind = kindDestinationScrollTo;
    res->zoom = zoom;
    return res;
}

bool IPageElement::Is(Kind expectedKind) {
    return kind == expectedKind;
}

Kind kindTocFzOutline = "tocFzOutline";
Kind kindTocFzOutlineAttachment = "tocFzOutlineAttachment";
Kind kindTocFzLink = "tocFzLink";
Kind kindTocDjvu = "tocDjvu";

// Sanitize a string for display in a single-line tree-view control (e.g. a
// bookmark/TOC label): drop soft hyphens and turn control chars / line
// separators into spaces, so they don't render as a stray hyphen or as
// boxes (#2647).
static TempStr CleanupTreeViewControlStringTemp(Str s) {
    if (!s) {
        return {};
    }
    TempWStr ws = ToWStrTemp(s);
    // soft hyphen (U+00AD): an invisible line-break hint, but rendered as a
    // visible hyphen by some fonts
    wstr::RemoveCharsInPlace(ws, L"\x00ad");
    // control chars (incl. embedded newlines/tabs) and the Unicode line and
    // paragraph separators render as boxes in a single-line label
    for (int i = 0; i < ws.len; i++) {
        wchar_t c = ws.s[i];
        if (c < 0x20 || c == 0x7f || c == 0x2028 || c == 0x2029) {
            ws.s[i] = L' ';
        }
    }
    // collapse the runs of whitespace we just introduced (and trim)
    wstr::NormalizeWSInPlace(ws);
    return ToUtf8Temp(ws);
}

TocItem* AllocTocItem(Arena* arena, Str title, int pageNo) {
    auto* item = (TocItem*)AllocZero(arena, sizeof(TocItem));
    item->title = str::Dup(arena, CleanupTreeViewControlStringTemp(title));
    item->pageNo = pageNo;
    item->color = kColorUnset;
    return item;
}

void FreeTocItemRec(Arena* arena, TocItem* item) {
    if (!item) {
        return;
    }
    FreeTocItemRec(arena, item->child);
    if (!item->destNotOwned) {
        delete item->dest;
    }
    FreeTocItemRec(arena, item->next);
    Free(arena, item->title.s);
    Free(arena, item);
}

void TocItem::AddSibling(TocItem* sibling) {
    TocItem* currNext = next;
    next = sibling;
    sibling->next = currNext;
    sibling->parent = parent;
}

void TocItem::AddSiblingAtEnd(TocItem* sibling) {
    TocItem* item = this;
    while (item->next) {
        item = item->next;
    }
    item->next = sibling;
    sibling->parent = item->parent;
}

void TocItem::AddChild(TocItem* newChild) {
    TocItem* curr = child;
    child = newChild;
    newChild->parent = this;
    newChild->next = curr;
}

// returns the destination this ToC item points to or nullptr
// (the result is owned by the TocItem and MUST NOT be deleted)
// TODO: rename to GetDestination()
IPageDestination* TocItem::GetPageDestination() const {
    return dest;
}

int TocItem::ChildCount() {
    int n = 0;
    auto* node = child;
    while (node) {
        n++;
        node = node->next;
    }
    return n;
}

TocItem* TocItem::ChildAt(int n) {
    if (n == 0) {
        currChild = child;
        currChildNo = 0;
        return child;
    }
    // speed up sequential iteration over children
    if (currChild != nullptr && n == currChildNo + 1) {
        currChild = currChild->next;
        ++currChildNo;
        return currChild;
    }
    auto* node = child;
    while (n > 0) {
        n--;
        node = node->next;
    }
    return node;
}

bool TocItem::IsExpanded() {
    // leaf items cannot be expanded
    if (child == nullptr) {
        return false;
    }
    // item is expanded when:
    // - expanded by default, not toggled (true, false)
    // - not expanded by default, toggled (false, true)
    // which boils down to:
    return isOpenDefault != isOpenToggled;
}

bool TocItem::PageNumbersMatch() const {
    int destPageNo = PageDestGetPageNo(dest);
    if (destPageNo <= 0) {
        return true; // TODO: should be false?
    }
    if (pageNo != destPageNo) {
        logf("pageNo: %d, dest->pageNo: %d\n", pageNo, destPageNo);
        return false;
    }
    return true;
}

TocTree::TocTree(TocItem* root) {
    this->root = root;
}

TocTree::~TocTree() {
    FreeTocItemRec(nullptr, root);
}

// TreeModel
TreeItem TocTree::Root() {
    return (TreeItem)root;
}

Str TocTree::Text(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return tocItem->title;
}

TreeItem TocTree::Parent(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return (TreeItem)tocItem->parent;
}

int TocTree::ChildCount(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return tocItem->ChildCount();
}

TreeItem TocTree::ChildAt(TreeItem ti, int idx) {
    auto* tocItem = (TocItem*)ti;
    return (TreeItem)tocItem->ChildAt(idx);
}

bool TocTree::IsExpanded(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return tocItem->IsExpanded();
}

bool TocTree::IsChecked(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return !tocItem->isUnchecked;
}

void TocTree::SetHandle(TreeItem ti, HTREEITEM hItem) {
    ReportIf(ti < 0);
    TocItem* tocItem = (TocItem*)ti;
    tocItem->hItem = hItem;
}

HTREEITEM TocTree::GetHandle(TreeItem ti) {
    ReportIf(ti < 0);
    TocItem* tocItem = (TocItem*)ti;
    return tocItem->hItem;
}

// TODO: speed up by removing recursion
static bool VisitTocTree(TocItem* ti, const VisitTocTreeCb& f) {
    bool cont;
    VisitTocTreeData d;
    while (ti) {
        d.ti = ti;
        f.Call(&d);
        cont = !d.stopTraversal;
        if (cont && ti->child) {
            cont = VisitTocTree(ti->child, f);
        }
        if (!cont) {
            return false;
        }
        ti = ti->next;
    }
    return true;
}

static bool VisitTocTreeWithParentRecursive(TocItem* ti, TocItem* parent, const VisitTocTreeCb& f) {
    bool cont;
    VisitTocTreeData d;
    while (ti) {
        d.ti = ti;
        d.parent = parent;
        f.Call(&d);
        cont = !d.stopTraversal;
        if (cont && ti->child) {
            cont = VisitTocTreeWithParentRecursive(ti->child, ti, f);
        }
        if (!cont) {
            return false;
        }
        ti = ti->next;
    }
    return true;
}

RenderPageArgs::RenderPageArgs(int pageNo, float zoom, int rotation, RectF* pageRect, RenderTarget target,
                               AbortCookie** cookie_out) {
    this->pageNo = pageNo;
    this->zoom = zoom;
    this->rotation = rotation;
    this->pageRect = pageRect;
    this->target = target;
    this->cookie_out = cookie_out;
}

int EngineBase::AddRef() {
    return AtomicRefCountAdd(&refCount);
}

// return true if deleted the object
bool EngineBase::Release() {
    int rc = AtomicRefCountDec(&refCount);
    if (rc == 0) {
        delete this;
        return true;
    }
    return false;
}

EngineBase::EngineBase() {
    arena = ArenaNew();
}

// document errors (mupdf warnings/errors may arrive from render threads)
void EngineBase::AppendError(Str msg) {
    ScopedMutex scope(&errorsLock);
    errors.Append(msg);
}

bool EngineBase::HasErrors() {
    ScopedMutex scope(&errorsLock);
    return !errors.IsEmpty();
}

// internal builder buffer (no copy); valid until next AppendError or engine
// destruction — do not free or keep beyond the current frame
TempStr EngineBase::GetErrorsTextTemp() {
    ScopedMutex scope(&errorsLock);
    return ToStr(errors);
}

EngineBase::~EngineBase() {
    if (pagesText) {
        for (int i = 0; i < pageCount; i++) {
            PageText* pt = &pagesText[i];
            free(pt->coords);
            str::Free(pt->text);
        }
        free(pagesText);
    }
    free(pagesTextState);
    str::Free(defaultExt);
    LogArenaStats(StrL("engine"), arena);
    ArenaDelete(arena);
}

struct TextExtractionThreadData {
    EngineBase* engine = nullptr;
    int pageNo = 0;
};

static void ExtractTextThread(TextExtractionThreadData* data) {
    data->engine->GetTextForPage(data->pageNo);
    data->engine->ReleaseTextExtractionThreadContext();
    data->engine->Release();
    delete data;
    AtomicIntDec(&gDangerousThreadCount);
}

// cached per-page text. First call on a page extracts text and caches it,
// subsequent calls return the cached copy. The returned pointers are owned
// by EngineBase and remain valid for the lifetime of the engine.
bool EngineBase::HasTextForPage(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        return false;
    }
    ScopedMutex scope(&textCacheLock);
    if (!pagesText) {
        return false;
    }
    PageText* pt = &pagesText[pageNo - 1];
    return (bool)pt->text;
}

TextExtractionState EngineBase::GetTextExtractionState(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        return TextExtractionState::Finished;
    }
    ScopedMutex scope(&textCacheLock);
    if (!pagesTextState) {
        return TextExtractionState::NotExtracted;
    }
    return pagesTextState[pageNo - 1];
}

void EngineBase::RequestTextExtraction(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        return;
    }

    {
        ScopedMutex scope(&textCacheLock);
        if (!pagesText) {
            pagesText = AllocArray<PageText>(pageCount);
        }
        if (!pagesTextState) {
            pagesTextState = AllocArray<TextExtractionState>(pageCount);
        }
        PageText* pt = &pagesText[pageNo - 1];
        if (pt->text || pagesTextState[pageNo - 1] != TextExtractionState::NotExtracted) {
            return;
        }
        pagesTextState[pageNo - 1] = TextExtractionState::Pending;
    }

    AddRef();
    AtomicIntInc(&gDangerousThreadCount);
    auto* data = new TextExtractionThreadData();
    data->engine = this;
    data->pageNo = pageNo;
    auto fn = MkFunc0<TextExtractionThreadData>(ExtractTextThread, data);
    ThreadHandle thread = StartThread(fn, "ExtractPageText");
    if (thread) {
        SafeCloseThreadHandle(&thread);
        return;
    }

    {
        ScopedMutex scope(&textCacheLock);
        if (pagesTextState && !pagesText[pageNo - 1].text) {
            pagesTextState[pageNo - 1] = TextExtractionState::NotExtracted;
        }
    }
    AtomicIntDec(&gDangerousThreadCount);
    Release();
    delete data;
}

// default always succeeds; EngineMupdf fails when locks are contended
bool EngineBase::TryExtractPageText(int pageNo, PageText* out) {
    *out = ExtractPageText(pageNo);
    return true;
}

// like GetElements but returns false (and no elements) if the engine can't
// acquire locks without blocking. Default always succeeds; EngineMupdf fails
// when a render thread holds them
bool EngineBase::TryGetElements(int pageNo, Vec<IPageElement*>* out) {
    *out = GetElements(pageNo);
    return true;
}

static Str ReturnCachedPageText(PageText* pt, int* lenOut, Rect** coordsOut) {
    if (lenOut) {
        *lenOut = pt->nCodepoints;
    }
    if (coordsOut) {
        *coordsOut = pt->coords;
    }
    Str text = pt->text;
    if (text.s) {
        text.len = pt->len;
        // str::Builder-backed buffers reserve a NUL slot at .len
        if (text.len >= 0) {
            text.s[text.len] = 0;
        }
    }
    return text;
}

// like GetTextForPage but returns false (and empty text) if the engine
// can't acquire locks without blocking (e.g. render thread is busy)
bool EngineBase::TryGetTextForPage(int pageNo, int* lenOut, Rect** coordsOut) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        if (lenOut) {
            *lenOut = 0;
        }
        if (coordsOut) {
            *coordsOut = nullptr;
        }
        return true;
    }

    bool extract = false;
    {
        ScopedMutex scope(&textCacheLock);
        if (!pagesText) {
            pagesText = AllocArray<PageText>(pageCount);
        }
        if (!pagesTextState) {
            pagesTextState = AllocArray<TextExtractionState>(pageCount);
        }
        if (pagesTextState[pageNo - 1] != TextExtractionState::Finished) {
            extract = true;
        }
    }

    if (extract) {
        PageText extracted;
        if (!TryExtractPageText(pageNo, &extracted)) {
            if (lenOut) {
                *lenOut = 0;
            }
            if (coordsOut) {
                *coordsOut = nullptr;
            }
            return false;
        }
        EnsurePageText(&extracted);

        ScopedMutex scope(&textCacheLock);
        PageText* pt = &pagesText[pageNo - 1];
        if (pagesTextState[pageNo - 1] != TextExtractionState::Finished) {
            FreePageText(pt);
            *pt = extracted;
            extracted = PageText();
            pagesTextState[pageNo - 1] = TextExtractionState::Finished;
        }
        FreePageText(&extracted);
    }

    ScopedMutex scope(&textCacheLock);
    PageText* pt = &pagesText[pageNo - 1];
    ReturnCachedPageText(pt, lenOut, coordsOut);
    return true;
}

Str EngineBase::GetTextForPage(int pageNo, int* lenOut, Rect** coordsOut) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        if (lenOut) {
            *lenOut = 0;
        }
        if (coordsOut) {
            *coordsOut = nullptr;
        }
        return {};
    }

    bool extract = false;
    {
        ScopedMutex scope(&textCacheLock);
        if (!pagesText) {
            pagesText = AllocArray<PageText>(pageCount);
        }
        if (!pagesTextState) {
            pagesTextState = AllocArray<TextExtractionState>(pageCount);
        }
        // Finished covers textless pages too (the page's text can stay empty). Pending
        // means a background thread was started by RequestTextExtraction but
        // selection still needs a synchronous extract here.
        if (pagesTextState[pageNo - 1] != TextExtractionState::Finished) {
            pagesTextState[pageNo - 1] = TextExtractionState::Pending;
            extract = true;
        }
    }

    if (extract) {
        PageText extracted = ExtractPageText(pageNo);
        EnsurePageText(&extracted);

        ScopedMutex scope(&textCacheLock);
        PageText* pt = &pagesText[pageNo - 1];
        if (pagesTextState[pageNo - 1] != TextExtractionState::Finished) {
            FreePageText(pt);
            *pt = extracted;
            extracted = PageText();
            pagesTextState[pageNo - 1] = TextExtractionState::Finished;
        }
        FreePageText(&extracted);
    }

    ScopedMutex scope(&textCacheLock);
    PageText* pt = &pagesText[pageNo - 1];
    return ReturnCachedPageText(pt, lenOut, coordsOut);
}

// number of pages the loaded document contains
int EngineBase::PageCount() const {
    ReportIf(pageCount < 0);
    return pageCount;
}

// the box inside PageMediabox that actually contains any relevant content
// (used for auto-cropping in Fit Content mode, can be PageMediabox)
RectF EngineBase::PageContentBox(int pageNo, RenderTarget /*target*/) {
    return PageMediabox(pageNo);
}

// the layout type this document's author suggests (if the user doesn't care)
// whether the content should be displayed as images instead of as document pages
// (e.g. with a black background and less padding in between and without search UI)
bool EngineBase::IsImageCollection() const {
    return isImageCollection;
}

// TODO: needs a more general interface
// whether it is allowed to print the current document
bool EngineBase::AllowsPrinting() const {
    return allowsPrinting;
}

// whether it is allowed to extract text from the current document
// (except for searching an accessibility reasons)
bool EngineBase::AllowsCopyingText() const {
    return allowsCopyingText;
}

// the DPI for a file is needed when converting internal measures to physical ones
float EngineBase::GetFileDPI() const {
    return fileDPI;
}

// creates a PageDestination from a name (or nullptr for invalid names)
// caller must delete the result
IPageDestination* EngineBase::GetNamedDest(Str /*name*/) {
    return nullptr;
}

// checks whether this document has an associated Table of Contents
bool EngineBase::HasToc() {
    TocTree* tree = GetToc();
    return tree != nullptr;
}

// returns the root element for the loaded document's Table of Contents
// caller must delete the result (when no longer needed)
TocTree* EngineBase::GetToc() {
    return nullptr;
}

#include "DocProperties.h"

// default implementation that just sets wanted keys
// keys are names of properties the caller wants. If given, we append those
// proerties in this order and potentially add more
// if keys are empty, we put them in order we want
void EngineBase::GetProperties(Props& propsOut) {
    for (int i = 0;; i++) {
        DocProp prop = gAllProps[i];
        if (prop == DocProp::None) {
            break;
        }
        // font list is loaded asynchronously in ShowProperties()
        if (prop == DocProp::FontList) {
            continue;
        }
        TempStr val = GetPropertyTemp(prop);
        if (len(val) == 0) continue;
        AddProp(propsOut, prop, val);
    }
}

// checks whether this document has explicit labels for pages (such as
// roman numerals) instead of the default plain arabic numbering
bool EngineBase::HasPageLabels() const {
    return hasPageLabels;
}

// returns a label to be displayed instead of the page number
// caller must free() the result
TempStr EngineBase::GetPageLabeTemp(int pageNo) const {
    return fmt("%d", pageNo);
}

// reverts GetPageLabel by returning the first page number having the given label
int EngineBase::GetPageByLabel(Str label) const {
    return ParseInt(label);
}

// whether this document required a password in order to be loaded
bool EngineBase::IsPasswordProtected() const {
    return isPasswordProtected;
}

// the name of the file this engine handles
Str EngineBase::FilePath() const {
    return fileNameBase;
}

RenderedBitmap* EngineBase::GetImageForPageElement(IPageElement* /*ipel*/) {
    CrashMe();
    return nullptr;
}

// Encoded file bytes of a page-element image, when the engine still has them
// (a JPEG stream in a PDF, a page of a CBZ, …). Empty if the image only
// exists as decoded pixels. Caller must str::Free.
Str EngineBase::GetImageDataForPageElement(IPageElement*) {
    return {};
}

// protected:
void EngineBase::SetFilePath(Str s) {
    fileNameBase = s ? str::Dup(arena, s) : Str();
}

// applies zoom and rotation to a point in user/page space converting
// it into device/screen space - or in the inverse direction
PointF EngineBase::Transform(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    RectF rc = RectF(pt, SizeF());
    RectF rect = Transform(rc, pageNo, zoom, rotation, inverse);
    return rect.TL();
}

// returns false if didn't perform action (temporary until we move
// all code there)
bool EngineBase::HandleLink(IPageDestination* /*dest*/, ILinkHandler* /*linkHandler*/) {
    // if not implemented in derived classes
    return false;
}

bool SaveFileOrData(Str srcFilePath, Str data, Str dstFilePath) {
    if (srcFilePath) {
        bool ok = file::Copy(dstFilePath, srcFilePath, false);
        if (ok) {
            return true;
        }
    }
    if (len(data) == 0) {
        return false;
    }
    return file::WriteFile(dstFilePath, data);
}
