
/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"

#include "utils/Log.h"

bool IsExternalUrl(const WCHAR* url) {
    return str::StartsWithI(url, L"http://") || str::StartsWithI(url, L"https://") || str::StartsWithI(url, L"mailto:");
}

bool IsExternalUrl(const char* url) {
    return str::StartsWithI(url, "http://") || str::StartsWithI(url, "https://") || str::StartsWithI(url, "mailto:");
}

void FreePageText(PageText* pageText) {
    str::Free(pageText->text);
    free((void*)pageText->coords);
    pageText->text = nullptr;
    pageText->coords = nullptr;
    pageText->len = 0;
}

Kind kindPageElementDest = "dest";
Kind kindPageElementImage = "image";
Kind kindPageElementComment = "comment";

Kind kindDestinationNone = "none";
Kind kindDestinationScrollTo = "scrollTo";
Kind kindDestinationLaunchURL = "launchURL";
Kind kindDestinationLaunchEmbedded = "launchEmbedded";
Kind kindDestinationLaunchFile = "launchFile";
Kind kindDestinationDjVu = "destinationDjVu";
Kind kindDestinationMupdf = "destinationMupdf";

static Kind destKinds[] = {kindDestinationNone,           kindDestinationScrollTo,   kindDestinationLaunchURL,
                           kindDestinationLaunchEmbedded, kindDestinationLaunchFile, kindDestinationDjVu,
                           kindDestinationMupdf};

PageDestination::~PageDestination() {
    free(value);
    free(name);
}

// string value associated with the destination (e.g. a path or a URL)
WCHAR* PageDestination::GetValue() {
    return value;
}

// the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
// (mainly applicable for links of type "LaunchFile" to PDF documents)
WCHAR* PageDestination::GetName() {
    return name;
}

IPageDestination* NewSimpleDest(int pageNo, RectF rect, float zoom, const WCHAR* value) {
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
    Kind kind = GetKind();
    return kind == expectedKind;
}

Kind kindTocFzOutline = "tocFzOutline";
Kind kindTocFzOutlineAttachment = "tocFzOutlineAttachment";
Kind kindTocFzLink = "tocFzLink";
Kind kindTocDjvu = "tocDjvu";

TocItem::TocItem(TocItem* parent, const WCHAR* title, int pageNo) {
    this->title = str::Dup(title);
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
    str::Free(rawVal1);
    str::Free(rawVal2);
    str::Free(engineFilePath);
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
    TocItem* currChild = child;
    child = newChild;
    newChild->parent = this;
    newChild->next = currChild;
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
    if (!dest || dest->GetPageNo() <= 0) {
        return true;
    }
    if (pageNo != dest->GetPageNo()) {
        logf("pageNo: %d, dest->pageNo: %d\n", pageNo, dest->GetPageNo());
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

WCHAR* TocTree::Text(TreeItem ti) {
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
    CrashIf(ti < 0);
    TocItem* tocItem = (TocItem*)ti;
    tocItem->hItem = hItem;
}

HTREEITEM TocTree::GetHandle(TreeItem ti) {
    CrashIf(ti < 0);
    TocItem* tocItem = (TocItem*)ti;
    return tocItem->hItem;
}

// TODO: speed up by removing recursion
bool VisitTocTree(TocItem* ti, const std::function<bool(TocItem*)>& f) {
    bool cont;
    while (ti) {
        cont = f(ti);
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

static bool VisitTocTreeWithParentRecursive(TocItem* ti, TocItem* parent,
                                            const std::function<bool(TocItem* ti, TocItem* parent)>& f) {
    bool cont;
    while (ti) {
        cont = f(ti, parent);
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

bool VisitTocTreeWithParent(TocItem* ti, const std::function<bool(TocItem* ti, TocItem* parent)>& f) {
    return VisitTocTreeWithParentRecursive(ti, nullptr, f);
}

static bool setTocItemParent(TocItem* ti, TocItem* parent) {
    ti->parent = parent;
    return true;
}

void SetTocTreeParents(TocItem* treeRoot) {
    VisitTocTreeWithParent(treeRoot, setTocItemParent);
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

EngineBase::~EngineBase() {
    free(decryptionKey);
}

int EngineBase::PageCount() const {
    CrashIf(pageCount < 0);
    return pageCount;
}

RectF EngineBase::PageContentBox(int pageNo, RenderTarget) {
    return PageMediabox(pageNo);
}

bool EngineBase::SaveFileAsPDF(const char*) {
    return false;
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

IPageDestination* EngineBase::GetNamedDest(const WCHAR*) {
    return nullptr;
}

bool EngineBase::HacToc() {
    TocTree* tree = GetToc();
    return tree != nullptr;
}

TocTree* EngineBase::GetToc() {
    return nullptr;
}

bool EngineBase::HasPageLabels() const {
    return hasPageLabels;
}

WCHAR* EngineBase::GetPageLabel(int pageNo) const {
    return str::Format(L"%d", pageNo);
}

int EngineBase::GetPageByLabel(const WCHAR* label) const {
    return _wtoi(label);
}

bool EngineBase::IsPasswordProtected() const {
    return isPasswordProtected;
}

char* EngineBase::GetDecryptionKey() const {
    return str::Dup(decryptionKey);
}

const WCHAR* EngineBase::FileName() const {
    return fileNameBase.Get();
}

RenderedBitmap* EngineBase::GetImageForPageElement(IPageElement*) {
    CrashMe();
    return nullptr;
}

void EngineBase::SetFileName(const WCHAR* s) {
    fileNameBase.SetCopy(s);
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

// skip file:// and maybe file:/// from s. It might be added by mupdf.
// do not free the result
static const WCHAR* SkipFileProtocolTemp(const WCHAR* s) {
    if (!str::StartsWithI(s, L"file://")) {
        return s;
    }
    s += 7; // skip "file://"
    while (*s == L'/') {
        s++;
    }
    return s;
}

// s could be in format "file://path.pdf#page=1"
// We only want the "path.pdf"
// caller must free
// TODO: could also parse page=1 and return it so that
// we can go to the right place
WCHAR* CleanupFileURL(const WCHAR* s) {
    s = SkipFileProtocolTemp(s);
    WCHAR* s2 = str::Dup(s);
    WCHAR* s3 = str::FindChar(s2, L'#');
    if (s3) {
        *s3 = 0;
    }
    return s2;
}
