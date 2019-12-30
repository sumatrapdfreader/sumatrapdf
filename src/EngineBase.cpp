
/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"

#include "EngineBase.h"

Kind kindPageElementDest = "dest";
Kind kindPageElementImage = "image";
Kind kindPageElementComment = "comment";

Kind kindDestinationNone = "none";
Kind kindDestinationScrollTo = "scrollTo";
Kind kindDestinationLaunchURL = "launchURL";
Kind kindDestinationLaunchEmbedded = "launchEmbedded";
Kind kindDestinationLaunchFile = "launchFile";
Kind kindDestinationNextPage = "nextPage";
Kind kindDestinationPrevPage = "prevPage";
Kind kindDestinationFirstPage = "firstPage";
Kind kindDestinationLastPage = "lastPage";
Kind kindDestinationFindDialog = "findDialog";
Kind kindDestinationFullScreen = "fullscreen";
Kind kindDestinationGoBack = "goBack";
Kind kindDestinationGoForward = "goForward";
Kind kindDestinationGoToPageDialog = "goToPageDialog";
Kind kindDestinationPrintDialog = "printDialog";
Kind kindDestinationSaveAsDialog = "saveAsDialog";
Kind kindDestinationZoomToDialog = "zoomToDialog";

PageDestination::~PageDestination() {
    free(value);
    free(name);
}

Kind PageDestination::Kind() const {
    return kind;
}

// page the destination points to (0 for external destinations such as URLs)
int PageDestination::GetPageNo() const {
    return pageNo;
}

// rectangle of the destination on the above returned page
RectD PageDestination::GetRect() const {
    return rect;
}

// string value associated with the destination (e.g. a path or a URL)
WCHAR* PageDestination::GetValue() const {
    return value;
}

// the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
// (mainly applicable for links of type "LaunchFile" to PDF documents)
WCHAR* PageDestination::GetName() const {
    return name;
}

PageElement::~PageElement() {
    free(value);
    delete dest;
}

// the type of this page element
bool PageElement::Is(Kind expectedKind) const {
    return kind == expectedKind;
}

// page this element lives on (0 for elements in a ToC)
int PageElement::GetPageNo() const {
    return pageNo;
}

// rectangle that can be interacted with
RectD PageElement::GetRect() const {
    return rect;
}

// string value associated with this element (e.g. displayed in an infotip)
// caller must free() the result
WCHAR* PageElement::GetValue() const {
    return value;
}

// if this element is a link, this returns information about the link's destination
// (the result is owned by the PageElement and MUST NOT be deleted)
PageDestination* PageElement::AsLink() {
    return dest;
}

RenderedBitmap::~RenderedBitmap() {
    DeleteObject(hbmp);
}

RenderedBitmap* RenderedBitmap::Clone() const {
    HBITMAP hbmp2 = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, size.dx, size.dy, 0);
    return new RenderedBitmap(hbmp2, size);
}

// render the bitmap into the target rectangle (streching and skewing as requird)
bool RenderedBitmap::StretchDIBits(HDC hdc, RectI target) const {
    return BlitHBITMAP(hbmp, hdc, target);
}

// callers must not delete this (use Clone if you have to modify it)
HBITMAP RenderedBitmap::GetBitmap() const {
    return hbmp;
}

SizeI RenderedBitmap::Size() const {
    return size;
}

DocTocItem::DocTocItem(const WCHAR* title, int pageNo) {
    this->title = str::Dup(title);
    this->pageNo = pageNo;
}

DocTocItem::~DocTocItem() {
    delete child;
    delete dest;
    while (next) {
        DocTocItem* tmp = next->next;
        next->next = nullptr;
        delete next;
        next = tmp;
    }
    free(title);
}

void DocTocItem::AddSibling(DocTocItem* sibling) {
    DocTocItem* item = this;
    while (item->next) {
        item = item->next;
    }
    item->next = sibling;
}

void DocTocItem::OpenSingleNode() {
    // only open (root level) ToC nodes if there's at most two
    if (next && next->next) {
        return;
    }

    if (!IsExpanded()) {
        isOpenToggled = !isOpenToggled;
    }
    if (!next) {
        return;
    }
    if (!next->IsExpanded()) {
        next->isOpenToggled = !next->isOpenToggled;
    }
}

// returns the destination this ToC item points to or nullptr
// (the result is owned by the DocTocItem and MUST NOT be deleted)
// TODO: rename to GetDestination()
PageDestination* DocTocItem::GetPageDestination() {
    return dest;
}

DocTocItem* CloneDocTocItemRecur(DocTocItem* ti) {
    if (ti == nullptr) {
        return nullptr;
    }
    DocTocItem* res = new DocTocItem();
    res->title = str::Dup(ti->title);
    res->isOpenDefault = ti->isOpenDefault;
    res->isOpenToggled = ti->isOpenToggled;
    res->isChecked = ti->isChecked;
    res->pageNo = ti->pageNo;
    res->id = ti->id;
    res->fontFlags = ti->fontFlags;
    res->color = ti->color;
    res->dest = clonePageDestination(ti->dest);
    res->child = CloneDocTocItemRecur(ti->child);
    res->next = CloneDocTocItemRecur(ti->next);
    return res;
}

DocTocTree* CloneDocTocTree(DocTocTree* tree) {
    DocTocTree* res = new DocTocTree();
    res->name = str::Dup(tree->name);
    res->root = CloneDocTocItemRecur(tree->root);
    return res;
}

PageDestination* newSimpleDest(int pageNo, RectD rect, const WCHAR* value) {
    auto res = new PageDestination();
    res->pageNo = pageNo;
    res->rect = rect;
    res->kind = kindDestinationScrollTo;
    if (value) {
        res->kind = kindDestinationLaunchURL;
        res->value = str::Dup(value);
    }
    return res;
}

PageDestination* clonePageDestination(PageDestination* dest) {
    if (!dest) {
        return nullptr;
    }
    auto res = new PageDestination();
    CrashIf(!dest->kind);
    res->kind = dest->kind;
    res->pageNo = dest->GetPageNo();
    res->rect = dest->GetRect();
    res->value = str::Dup(dest->GetValue());
    res->name = str::Dup(dest->GetName());
    return res;
}

PageElement* clonePageElement(PageElement* el) {
    if (!el) {
        return nullptr;
    }
    auto* res = new PageElement();
    res->kind = el->kind;
    res->pageNo = el->pageNo;
    res->rect = el->rect;
    res->value = str::Dup(el->value);
    res->dest = clonePageDestination(el->dest);
    return res;
}

WCHAR* DocTocItem::Text() {
    return title;
}

TreeItem* DocTocItem::Parent() {
    // don't use it
    CrashMe();
    return nullptr;
}

int DocTocItem::ChildCount() {
    int n = 0;
    auto node = child;
    while (node) {
        n++;
        node = node->next;
    }
    return n;
}

TreeItem* DocTocItem::ChildAt(int n) {
    auto node = child;
    while (n > 0) {
        node = node->next;
        n--;
    }
    return node;
}

bool DocTocItem::IsExpanded() {
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

bool DocTocItem::IsChecked() {
    return isChecked;
}

DocTocTree::DocTocTree(DocTocItem* root) {
    this->root = root;
}

DocTocTree::~DocTocTree() {
    free(name);
    delete root;
}

int DocTocTree::RootCount() {
    int n = 0;
    auto node = root;
    while (node) {
        n++;
        node = node->next;
    }
    return n;
}

TreeItem* DocTocTree::RootAt(int n) {
    auto node = root;
    while (n > 0) {
        node = node->next;
        n--;
    }
    return node;
}
