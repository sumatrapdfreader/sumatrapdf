
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
    return str::StartsWithI(url, "http://") || str::StartsWithI(url, "https://") || str::StartsWithI(url, "mailto:");
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
    auto res = new PageDestination();
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
TempStr CleanupTreeViewControlStringTemp(Str s) {
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

TocItem::TocItem(TocItem* parent, Str title, int pageNo) {
    this->title = str::Dup(CleanupTreeViewControlStringTemp(title));
    this->pageNo = pageNo;
    this->parent = parent;
}

TocItem::~TocItem() {
    delete child;
    if (!destNotOwned) {
        delete dest;
    }
    while (next) {
        TocItem* tmp = next->next;
        next->next = nullptr;
        delete next;
        next = tmp;
    }
    str::Free(title);
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

// regular delete is recursive, this deletes only this item
void TocItem::DeleteJustSelf() {
    child = nullptr;
    next = nullptr;
    parent = nullptr;
    delete this;
}

// returns the destination this ToC item points to or nullptr
// (the result is owned by the TocItem and MUST NOT be deleted)
// TODO: rename to GetDestination()
IPageDestination* TocItem::GetPageDestination() const {
    return dest;
}

int TocItem::ChildCount() {
    int n = 0;
    auto node = child;
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
    auto node = child;
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
    delete root;
}

TreeItem TocTree::Root() {
    return (TreeItem)root;
}

Str TocTree::Text(TreeItem ti) {
    auto tocItem = (TocItem*)ti;
    return tocItem->title;
}

TreeItem TocTree::Parent(TreeItem ti) {
    auto tocItem = (TocItem*)ti;
    return (TreeItem)tocItem->parent;
}

int TocTree::ChildCount(TreeItem ti) {
    auto tocItem = (TocItem*)ti;
    return tocItem->ChildCount();
}

TreeItem TocTree::ChildAt(TreeItem ti, int idx) {
    auto tocItem = (TocItem*)ti;
    return (TreeItem)tocItem->ChildAt(idx);
}

bool TocTree::IsExpanded(TreeItem ti) {
    auto tocItem = (TocItem*)ti;
    return tocItem->IsExpanded();
}

bool TocTree::IsChecked(TreeItem ti) {
    auto tocItem = (TocItem*)ti;
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
    auto data = new TextExtractionThreadData();
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

bool EngineBase::TryExtractPageText(int pageNo, PageText* out) {
    *out = ExtractPageText(pageNo);
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
        PageText* pt = &pagesText[pageNo - 1];
        // Finished covers textless pages too (pt->text can stay empty). Pending
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

int EngineBase::PageCount() const {
    ReportIf(pageCount < 0);
    return pageCount;
}

RectF EngineBase::PageContentBox(int pageNo, RenderTarget) {
    return PageMediabox(pageNo);
}

bool EngineBase::IsImageCollection() const {
    return isImageCollection;
}

bool EngineBase::AllowsPrinting() const {
    return allowsPrinting;
}

bool EngineBase::AllowsCopyingText() const {
    return allowsCopyingText;
}

float EngineBase::GetFileDPI() const {
    return fileDPI;
}

IPageDestination* EngineBase::GetNamedDest(Str) {
    return nullptr;
}

bool EngineBase::HasToc() {
    TocTree* tree = GetToc();
    return tree != nullptr;
}

TocTree* EngineBase::GetToc() {
    return nullptr;
}

#include "DocProperties.h"

// default implementation that just sets wanted keys
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
        if (val) {
            AddProp(propsOut, prop, val);
        }
    }
}

bool EngineBase::HasPageLabels() const {
    return hasPageLabels;
}

TempStr EngineBase::GetPageLabeTemp(int pageNo) const {
    return fmt("%d", pageNo);
}

int EngineBase::GetPageByLabel(Str label) const {
    return ParseInt(label);
}

bool EngineBase::IsPasswordProtected() const {
    return isPasswordProtected;
}

Str EngineBase::FilePath() const {
    return fileNameBase;
}

RenderedBitmap* EngineBase::GetImageForPageElement(IPageElement*) {
    CrashMe();
    return nullptr;
}

void EngineBase::SetFilePath(Str s) {
    fileNameBase = s ? str::Dup(arena, s) : Str();
}

PointF EngineBase::Transform(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    RectF rc = RectF(pt, SizeF());
    RectF rect = Transform(rc, pageNo, zoom, rotation, inverse);
    return rect.TL();
}

bool EngineBase::HandleLink(IPageDestination*, ILinkHandler*) {
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
