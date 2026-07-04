/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dict.h"
#include "base/UITask.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/HtmlWindow.h"
#include "wingui/ChmDocView.h"
#include "wingui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"

#include "base/Log.h"

static IPageDestination* NewChmNamedDest(Str url, int pageNo) {
    if (!url) {
        return nullptr;
    }
    IPageDestination* dest = nullptr;
    if (IsExternalUrl(url)) {
        dest = new PageDestinationURL(url);
    } else {
        auto pdest = new PageDestination();
        pdest->kind = kindDestinationScrollTo;
        pdest->name = str::Dup(url);
        dest = pdest;
    }
    dest->pageNo = pageNo;
    ReportIf(!dest->kind);
    dest->rect = RectF(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    return dest;
}

static TocItem* NewChmTocItem(TocItem* parent, Str title, int pageNo, Str url) {
    auto res = new TocItem(parent, title, pageNo);
    res->dest = NewChmNamedDest(url, pageNo);
    return res;
}

class HtmlWindowHandler : public HtmlWindowCallback {
    ChmModel* cm;

  public:
    explicit HtmlWindowHandler(ChmModel* cm) : cm(cm) {}
    ~HtmlWindowHandler() override = default;

    bool OnBeforeNavigate(Str url, bool newWindow) override { return cm->OnBeforeNavigate(url, newWindow); }
    void OnDocumentComplete(Str url) override { cm->OnDocumentComplete(url); }
    void OnLButtonDown() override { cm->OnLButtonDown(); }
    Str GetDataForUrl(Str url) override { return cm->GetDataForUrl(url); }
    void DownloadData(Str url, Str data) override { cm->DownloadData(url, data); }
};

struct ChmTocTraceItem {
    Str title; // owned by ChmModel::poolAllocator
    Str url;   // owned by ChmModel::poolAllocator
    int level = 0;
    int pageNo = 0;
};

ChmModel::ChmModel(DocControllerCallback* cb) : DocController(cb) {
    InitializeCriticalSection(&docAccess);
    poolAlloc = ArenaNew();
}

ChmModel::~ChmModel() {
    EnterCriticalSection(&docAccess);
    // TODO: deleting htmlWindow seems to spin a modal loop which
    //       can lead to WM_PAINT being dispatched for the parent
    //       hwnd and then crashing in SumatraPDF.cpp's DrawDocument
    delete docView;
    delete htmlWindowCb;
    delete doc;
    delete tocTrace;
    delete tocTree;
    DeleteVecMembers(urlDataCache);
    LeaveCriticalSection(&docAccess);
    DeleteCriticalSection(&docAccess);
    ArenaDelete(poolAlloc);
    str::Free(fileName);
    str::Free(currentPageUrl);
}

Str ChmModel::GetFilePath() const {
    return fileName;
}

Str ChmModel::GetDefaultFileExt() const {
    return ".chm";
}

int ChmModel::PageCount() const {
    return len(pages);
}

TempStr ChmModel::GetPropertyTemp(Str name) {
    return doc->GetPropertyTemp(name);
}

int ChmModel::CurrentPageNo() const {
    return currentPageNo;
}

void ChmModel::GoToPage(int pageNo, bool) {
    ReportIf(!ValidPageNo(pageNo));
    if (!ValidPageNo(pageNo)) {
        return;
    }
    // re-display the exact current url (which may be a redirect/anchor not in
    // `pages`) so navigating to the same page preserves it
    if (pageNo == currentPageNo && len(currentPageUrl) > 0) {
        DisplayPage(currentPageUrl);
        return;
    }
    DisplayPage(pages.At(pageNo - 1));
}

bool ChmModel::SetParentHwnd(HWND hwnd) {
    // can be already set if tab was restored at startup and then switched away
    // without going through the normal CloseDocumentInCurrentTab path
    if (docView || htmlWindowCb) {
        RemoveParentHwnd();
    }
    htmlWindowCb = new HtmlWindowHandler(this);
    docView = ChmDocView::Create(hwnd, htmlWindowCb);
    if (!docView) {
        delete htmlWindowCb;
        htmlWindowCb = nullptr;
        return false;
    }
    return true;
}

void ChmModel::RemoveParentHwnd() {
    if (!docView && !htmlWindowCb) {
        return;
    }
    // remember where we were so it can be restored when the view is recreated
    // (e.g. when switching back to this tab)
    SaveHtmlScrollPos();
    restoreHtmlScrollPos = true;
    delete docView;
    docView = nullptr;
    delete htmlWindowCb;
    htmlWindowCb = nullptr;
}

void ChmModel::PrintCurrentPage(bool showUI) const {
    if (docView) {
        docView->PrintCurrentPage(showUI);
    }
}

void ChmModel::FindInCurrentPage() const {
    if (docView) {
        docView->FindInCurrentPage();
    }
}

void ChmModel::SelectAll() const {
    if (docView) {
        docView->SelectAll();
    }
}

void ChmModel::CopySelection() const {
    if (docView) {
        docView->CopySelection();
    }
}

static bool gSendingHtmlWindowMsg = false;

LRESULT ChmModel::PassUIMsg(UINT msg, WPARAM wp, LPARAM lp) const {
    if (!docView || gSendingHtmlWindowMsg) {
        return 0;
    }
    gSendingHtmlWindowMsg = true;
    auto res = docView->SendMsg(msg, wp, lp);
    gSendingHtmlWindowMsg = false;
    return res;
}

bool ChmModel::DisplayPage(Str pageUrl) {
    if (!pageUrl) {
        return false;
    }
    // pageUrl may alias currentPageUrl (e.g. via GoToPage), which we overwrite
    // below with SetCopy(); take a stable copy so the later use of pageUrl
    // (NavigateToDataUrl) doesn't read freed memory
    pageUrl = str::DupTemp(pageUrl);
    if (IsExternalUrl(pageUrl)) {
        // open external links in an external browser
        // (same as for PDF, XPS, etc. documents)
        if (cb) {
            // TODO: optimize, create just destination
            auto item = NewChmTocItem(nullptr, nullptr, 0, pageUrl);
            cb->GotoLink(item->dest);
            delete item;
        }
        return true;
    }

    TempStr url = url::GetFullPathTemp(pageUrl);
    bool wasSameUrl = len(currentPageUrl) > 0 && str::Eq(currentPageUrl, url);
    int pageNo = pages.Find(url) + 1;
    // if we're reloading the same url to restore a scroll position, don't
    // clobber that saved position by saving the current (pre-restore) one
    bool restoreScrollAfterLoad = restoreHtmlScrollPos && wasSameUrl;
    if (!restoreScrollAfterLoad) {
        SaveHtmlScrollPos();
        skipNextBeforeNavigateScrollSave = true;
    }
    str::ReplaceWithCopy(&currentPageUrl, url);
    if (pageNo > 0) {
        currentPageNo = pageNo;
    }

    PointF savedPos;
    if (GetSavedHtmlScrollPosForUrl(url, &savedPos)) {
        htmlScrollPos = savedPos;
        restoreHtmlScrollPos = true;
    } else if (!restoreScrollAfterLoad) {
        restoreHtmlScrollPos = false;
    }

    // This is a hack that seems to be needed for some chm files where
    // url starts with "..\" even though it's not accepted by ie as
    // a correct its: url. There's a possibility it breaks some other
    // chm files (I don't know such cases, though).
    // A more robust solution would try to match with the actual
    // names of files inside chm package.
    if (str::StartsWith(pageUrl, "..\\")) {
        pageUrl = Str(pageUrl.s + 3, pageUrl.len - 3);
    }

    if (str::StartsWith(pageUrl, "/")) {
        pageUrl = Str(pageUrl.s + 1, pageUrl.len - 1);
    }

    if (!docView) {
        return false;
    }
    docView->NavigateToDataUrl(pageUrl);
    return true;
}

void ChmModel::ScrollTo(int pageNo, RectF rect, float zoom) {
    if (IsValidZoom(zoom)) {
        SetZoomVirtual(zoom, nullptr);
    }
    if (rect.x >= 0 || rect.y >= 0) {
        htmlScrollPos = PointF(rect.x, rect.y);
        restoreHtmlScrollPos = true;
        if (ValidPageNo(pageNo)) {
            SaveHtmlScrollPosForUrl(pages.At(pageNo - 1), htmlScrollPos);
        }
    }
    GoToPage(pageNo, false);
}

bool ChmModel::HandleLink(IPageDestination* link, ILinkHandler*) {
    Kind k = link->GetKind();
    if (k != kindDestinationScrollTo) {
        logf("ChmModel::HandleLink: unsupported kind '%s'\n", Str(k));
        ReportIfFast(link->GetKind() != kindDestinationScrollTo);
    }
    Str url = PageDestGetName(link);
    if (DisplayPage(url)) {
        return true;
    }
    int pageNo = PageDestGetPageNo(link);
    GoToPage(pageNo, false);
    return true;
}

bool ChmModel::CanNavigate(int dir) const {
    if (!docView) {
        return false;
    }
    if (dir < 0) {
        return docView->canGoBack;
    }
    return docView->canGoForward;
}

void ChmModel::Navigate(int dir) {
    if (!docView) {
        return;
    }

    if (dir < 0) {
        for (; dir < 0 && CanNavigate(dir); dir++) {
            docView->GoBack();
        }
    } else {
        for (; dir > 0 && CanNavigate(dir); dir--) {
            docView->GoForward();
        }
    }
}

void ChmModel::SetDisplayMode(DisplayMode, bool) {
    // no-op
}

DisplayMode ChmModel::GetDisplayMode() const {
    return DisplayMode::SinglePage;
}

void ChmModel::SetInPresentation(bool) {
    // no-op
}

void ChmModel::SetViewPortSize(Size) {
    // no-op
}

ChmModel* ChmModel::AsChm() {
    return this;
}

void ChmModel::SetZoomVirtual(float zoom, Point*) {
    if (zoom > 0) {
        zoom = limitValue(zoom, kZoomMin, kZoomMax);
    }
    if (zoom <= 0 || !IsValidZoom(zoom)) {
        zoom = 100.0f;
    }
    ZoomTo(zoom);
    zoomVirtual = zoom;
    initZoom = zoom;
}

// Save the current scroll position for the currently displayed url/page.
void ChmModel::SaveHtmlScrollPos() {
    if (!docView) {
        return;
    }
    Point pos = docView->GetScrollPos();
    if (pos.x < 0 && pos.y < 0) {
        return;
    }
    htmlScrollPos = PointF((float)pos.x, (float)pos.y);
    if (len(currentPageUrl) > 0) {
        SaveHtmlScrollPosForUrl(currentPageUrl, htmlScrollPos);
        return;
    }
    SaveHtmlScrollPosForPage(currentPageNo);
}

void ChmModel::SaveHtmlScrollPosForPage(int pageNo) {
    if (!ValidPageNo(pageNo)) {
        return;
    }
    SaveHtmlScrollPosForUrl(pages.At(pageNo - 1), htmlScrollPos);
}

void ChmModel::SaveHtmlScrollPosForUrl(Str url, PointF pos) {
    if (!url || pos.x < 0 || pos.y < 0) {
        return;
    }

    TempStr plainUrl = url::GetFullPathTemp(url);
    int idx = htmlScrollUrls.Find(plainUrl);
    if (idx >= 0) {
        htmlScrollPositions.At(idx) = pos;
        return;
    }

    htmlScrollUrls.Append(plainUrl);
    htmlScrollPositions.Append(pos);
}

bool ChmModel::GetSavedHtmlScrollPosForPage(int pageNo, PointF* pos) const {
    if (!pos || !ValidPageNo(pageNo)) {
        return false;
    }
    return GetSavedHtmlScrollPosForUrl(pages.At(pageNo - 1), pos);
}

bool ChmModel::GetSavedHtmlScrollPosForUrl(Str url, PointF* pos) const {
    if (!url || !pos) {
        return false;
    }

    TempStr plainUrl = url::GetFullPathTemp(url);
    int idx = htmlScrollUrls.Find(plainUrl);
    if (idx < 0) {
        return false;
    }

    *pos = htmlScrollPositions.At(idx);
    return pos->x >= 0 || pos->y >= 0;
}

void ChmModel::RestoreHtmlScrollPos() {
    if (!docView || !restoreHtmlScrollPos) {
        return;
    }
    restoreHtmlScrollPos = false;
    if (htmlScrollPos.x < 0 && htmlScrollPos.y < 0) {
        return;
    }
    int x = (int)htmlScrollPos.x;
    int y = (int)htmlScrollPos.y;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    docView->SetScrollPos(Point(x, y));
}

void ChmModel::ZoomTo(float zoomLevel) const {
    if (docView) {
        docView->SetZoomPercent((int)zoomLevel);
    }
}

float ChmModel::GetZoomVirtual(bool) const {
    if (!docView) {
        return 100;
    }
    return (float)docView->GetZoomPercent();
}

struct ChmTocBuilder : EbookTocVisitor {
    ChmFile* doc = nullptr;

    StrVec* pages = nullptr;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;
    Arena* allocator = nullptr;
    // TODO: could use dict::MapStrToInt instead of StrList in the caller as well
    dict::MapStrToInt urlsSet;

    // We fake page numbers by doing a depth-first traversal of
    // toc tree and considering each unique html page in toc tree
    // as a page
    int CreatePageNoForURL(Str url) {
        if (!url || IsExternalUrl(url)) {
            return 0;
        }

        TempStr plainUrl = url::GetFullPathTemp(url);
        int pageNo = len(*pages) + 1;
        bool inserted = urlsSet.Insert(plainUrl, pageNo, &pageNo);
        if (inserted) {
            pages->Append(plainUrl);
            ReportIf(pageNo != len(*pages));
        } else {
            ReportIf(pageNo == len(*pages) + 1);
        }
        return pageNo;
    }

  public:
    ChmTocBuilder(ChmFile* doc, StrVec* pages, Vec<ChmTocTraceItem>* tocTrace, Arena* allocator) {
        this->doc = doc;
        this->pages = pages;
        this->tocTrace = tocTrace;
        this->allocator = allocator;
        int n = len(*pages);
        for (int i = 0; i < n; i++) {
            Str url = pages->At(i);
            bool inserted = urlsSet.Insert(url, i + 1, nullptr);
            ReportIf(!inserted);
        }
    }

    void Visit(Str name, Str url, int level) override {
        Str nameDup = str::Dup(allocator, name);
        Str urlDup = str::Dup(allocator, url);
        int pageNo = CreatePageNoForURL(urlDup);
        ChmTocTraceItem item{nameDup, urlDup, level, pageNo};
        tocTrace->Append(item);
    }
};

bool ChmModel::Load(Str fileName) {
    str::ReplaceWithCopy(&this->fileName, fileName);
    doc = ChmFile::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    // always make the document's homepage page 1
    TempStr page = strconv::AnsiToUtf8(doc->GetHomePath());
    pages.Append(page);

    // parse the ToC here, since page numbering depends on it
    tocTrace = new Vec<ChmTocTraceItem>();
    ChmTocBuilder tmpTocBuilder(doc, &pages, tocTrace, poolAlloc);
    doc->ParseToc(&tmpTocBuilder);
    ReportIf(len(pages) == 0);
    return len(pages) > 0;
}

struct ChmCacheEntry {
    // owned by ChmModel::poolAllocator
    Str url;
    Str data;

    explicit ChmCacheEntry(Str url);
    ~ChmCacheEntry() { str::Free(data); };
};

ChmCacheEntry::ChmCacheEntry(Str url) {
    this->url = url;
}

ChmCacheEntry* ChmModel::FindDataForUrl(Str url) const {
    int n = len(urlDataCache);
    for (int i = 0; i < n; i++) {
        ChmCacheEntry* e = urlDataCache.at(i);
        if (str::Eq(url, e->url)) {
            return e;
        }
    }
    return nullptr;
}

// Called after html document has been loaded.
// Sync the state of the ui with the page (show
// the right page number, select the right item in toc tree)
void ChmModel::OnDocumentComplete(Str url) {
    if (!url || IsBlankUrl(url)) {
        return;
    }
    if (url.s[0] == '/') {
        url = Str(url.s + 1, url.len - 1);
    }
    TempStr toFind = url::GetFullPathTemp(url);
    str::ReplaceWithCopy(&currentPageUrl, toFind);
    int pageNo = pages.Find(toFind) + 1;
    if (pageNo > 0) {
        currentPageNo = pageNo;
    }

    PointF savedPos;
    if (GetSavedHtmlScrollPosForUrl(toFind, &savedPos)) {
        htmlScrollPos = savedPos;
        restoreHtmlScrollPos = true;
    }

    // TODO: setting zoom before the first page is loaded seems not to work
    // (might be a regression from between r4593 and r4629), so the intended
    // zoom is applied here instead. Re-apply it after *every* load: the hosted
    // control is recreated when switching tabs, which resets it to 100%.
    if (IsValidZoom(initZoom)) {
        zoomVirtual = initZoom;
        initZoom = kInvalidZoom;
    }
    ZoomTo(zoomVirtual);
    RestoreHtmlScrollPos();

    if (cb && pageNo > 0) {
        cb->PageNoChanged(this, pageNo);
    }
}

// Called before we start loading html for a given url. Will block
// loading if returns false.
bool ChmModel::OnBeforeNavigate(Str url, bool newWindow) {
    // save scroll pos of the page we're leaving, unless DisplayPage() already
    // saved it before triggering this programmatic navigation
    if (skipNextBeforeNavigateScrollSave) {
        skipNextBeforeNavigateScrollSave = false;
    } else {
        // user-initiated navigation (e.g. clicking a link): currentPageUrl still
        // refers to the page being left, so save its live scroll position
        SaveHtmlScrollPos();
    }

    // ensure that JavaScript doesn't keep the focus
    // in the HtmlWindow when a new page is loaded
    if (cb) {
        cb->FocusFrame(false);
    }

    if (!newWindow) {
        return true;
    }

    // don't allow new MSIE windows to be opened
    // instead pass the URL to the system's default browser
    if (url && cb) {
        // TODO: optimize, create just destination
        auto item = NewChmTocItem(nullptr, nullptr, 1, url);
        cb->GotoLink(item->dest);
        delete item;
    }
    return false;
}

// Load and cache data for a given url inside CHM file.
Str ChmModel::GetDataForUrl(Str url) {
    ScopedCritSec scope(&docAccess);
    TempStr plainUrl = url::GetFullPathTemp(url);
    ChmCacheEntry* e = FindDataForUrl(plainUrl);
    if (!e) {
        Str s = str::Dup(poolAlloc, plainUrl);
        e = new ChmCacheEntry(s);
        e->data = str::Dup(doc->GetDataTemp(plainUrl));
        if (str::IsEmpty(e->data)) {
            delete e;
            return {};
        }
        urlDataCache.Append(e);
    }
    return e->data;
}

void ChmModel::DownloadData(Str url, Str data) {
    if (!cb) {
        return;
    }
    cb->SaveDownload(url, data);
}

void ChmModel::OnLButtonDown() {
    if (cb) {
        cb->FocusFrame(true);
    }
}

// named destinations are either in-document URLs or Alias topic IDs
IPageDestination* ChmModel::GetNamedDest(Str name) {
    TempStr url = url::GetFullPathTemp(name);
    int pageNo = pages.Find(url) + 1;
    if (pageNo >= 1) {
        return NewChmNamedDest(url, pageNo);
    }
    if (doc->HasData(url)) {
        return NewChmNamedDest(url, 1);
    }
    unsigned int topicID;
    if (str::IsNull(str::Parse(name, "%u%$", &topicID))) {
        return nullptr;
    }
    TempStr topicURL = doc->ResolveTopicID(topicID);
    if (!topicURL) {
        return nullptr;
    }
    url = topicURL;
    if (!doc->HasData(url)) {
        return nullptr;
    }
    pageNo = pages.Find(url) + 1;
    if (pageNo < 1) {
        // some documents use redirection URLs which aren't listed in the ToC
        // return pageNo=1 for these, as HandleLink will ignore that anyway
        // but LinkHandler::ScrollTo doesn't
        pageNo = 1;
    }
    return NewChmNamedDest(url, pageNo);
}

TocTree* ChmModel::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    if (len(*tocTrace) == 0) {
        return nullptr;
    }

    TocItem* root = nullptr;
    bool foundRoot = false;
    TocItem** nextChild = &root;
    Vec<TocItem*> levels;
    int idCounter = 0;

    for (ChmTocTraceItem& ti : *tocTrace) {
        // TODO: set parent
        TocItem* item = NewChmTocItem(nullptr, ti.title, ti.pageNo, ti.url);
        item->id = ++idCounter;
        // append the item at the correct level
        ReportIf(ti.level < 1);
        if (ti.level <= len(levels)) {
            levels.RemoveAt(ti.level, len(levels) - ti.level);
            levels.Last()->AddSiblingAtEnd(item);
        } else {
            *nextChild = item;
            levels.Append(item);
            foundRoot = true;
        }
        nextChild = &item->child;
    }
    if (!foundRoot) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

// adapted from DisplayModel::NextZoomStep
float ChmModel::GetNextZoomStep(float towardsLevel) const {
    float currZoom = GetZoomVirtual(true);
    if (MaybeGetNextZoomByIncrement(&currZoom, towardsLevel)) {
        // chm uses browser control which only supports integer zoom levels
        // this ensures we're not stuck on a given zoom level i.e. advance by at least 1%
        int iCurrZoom2 = (int)GetZoomVirtual(true);
        int iCurrZoom = (int)currZoom;
        if (iCurrZoom == iCurrZoom2) {
            currZoom += 1.f;
        }
        return currZoom;
    }

    int nZoomLevels;
    float* zoomLevels = GetDefaultZoomLevels(&nZoomLevels);

    // chm uses browser control which only supports integer zoom levels
    // this ensures we're not stuck on a given zoom level
    // due to float => int truncation
    int iCurrZoom = (int)currZoom;
    int iTowardsLevel = (int)towardsLevel;
    int iNewZoom = iTowardsLevel;
    if (iCurrZoom < towardsLevel) {
        for (int i = 0; i < nZoomLevels; i++) {
            int iZoom = (int)zoomLevels[i];
            if (iZoom > iCurrZoom) {
                iNewZoom = iZoom;
                break;
            }
        }
    } else if (iCurrZoom > towardsLevel) {
        for (int i = nZoomLevels - 1; i >= 0; i--) {
            int iZoom = (int)zoomLevels[i];
            if (iZoom < iCurrZoom) {
                iNewZoom = iZoom;
                break;
            }
        }
    }

    return (float)iNewZoom;
}

void ChmModel::GetDisplayState(FileState* fs) {
    Str fileNameA = fileName;
    if (!fs->filePath || !str::EqI(fs->filePath, fileNameA)) {
        SetFileStatePath(fs, fileNameA);
    }

    fs->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    str::ReplaceWithCopy(&fs->displayMode, DisplayModeToString(GetDisplayMode()));
    ZoomToString(&fs->zoom, GetZoomVirtual(), fs);

    fs->pageNo = CurrentPageNo();
    SaveHtmlScrollPos();
    fs->scrollPos = htmlScrollPos;
}

struct ChmThumbnailTask : HtmlWindowCallback {
    ChmFile* doc = nullptr;
    HWND hwnd = nullptr;
    HtmlWindow* hw = nullptr;
    bool didSave = false;
    Size size;
    const OnBitmapRendered* saveThumbnail = nullptr;
    Str homeUrl;
    Vec<Str> data;
    CRITICAL_SECTION docAccess;

    ChmThumbnailTask(ChmFile* doc, HWND hwnd, Size size, const OnBitmapRendered* saveThumbnail);
    ~ChmThumbnailTask() override;
    void StartCreateThumbnail(HtmlWindow* hw);
    bool OnBeforeNavigate(Str, bool newWindow) override;
    void OnDocumentComplete(Str url) override;
    Str GetDataForUrl(Str url) override;
    void OnLButtonDown() override;
    void DownloadData(Str, Str) override;
};

static void SafeDeleteChmThumbnailTask(ChmThumbnailTask* d) {
    logf("SafeDeleteChmThumbnailTask: about to delete ChmThumbnailTask: 0x%p\n", (void*)d);
    delete d;
}

ChmThumbnailTask::ChmThumbnailTask(ChmFile* doc, HWND hwnd, Size size, const OnBitmapRendered* saveThumbnail) {
    this->doc = doc;
    this->hwnd = hwnd;
    this->size = size;
    this->saveThumbnail = saveThumbnail;
    this->didSave = false;
    InitializeCriticalSection(&docAccess);
}

ChmThumbnailTask::~ChmThumbnailTask() {
    EnterCriticalSection(&docAccess);
    delete hw;
    DestroyWindow(hwnd);
    delete doc;
    for (auto&& d : data) {
        str::Free(d);
    }
    LeaveCriticalSection(&docAccess);
    DeleteCriticalSection(&docAccess);
    delete saveThumbnail;
    str::Free(homeUrl);
}

bool ChmThumbnailTask::OnBeforeNavigate(Str, bool newWindow) {
    return !newWindow;
}

void ChmThumbnailTask::StartCreateThumbnail(HtmlWindow* hw) {
    this->hw = hw;
    homeUrl = strconv::AnsiToUtf8(doc->GetHomePath());
    if (str::StartsWith(homeUrl, "/")) {
        str::ReplaceWithCopy(&homeUrl, Str(homeUrl.s + 1));
    }
    hw->NavigateToDataUrl(homeUrl);
}

Str ChmThumbnailTask::GetDataForUrl(Str url) {
    ScopedCritSec scope(&docAccess);
    TempStr plainUrl = url::GetFullPathTemp(url);
    Str d = str::Dup(doc->GetDataTemp(plainUrl));
    data.Append(d);
    return d;
}

void ChmThumbnailTask::OnDocumentComplete(Str url) {
    if (url && url.s[0] == '/') {
        url = Str(url.s + 1, url.len - 1);
    }
    if (!str::Eq(url, homeUrl)) {
        return;
    }
    logf("ChmThumbnailTask::OnDocumentComplete: '%s'\n", url);
    if (didSave) {
        // don't crash creating .chm thumbnail
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4519
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4833
        return;
    }
    didSave = true;
    Rect area(0, 0, size.dx * 2, size.dy * 2);
    HBITMAP hbmp = hw->TakeScreenshot(area, size);
    if (hbmp) {
        RenderedBitmap* bmp = new RenderedBitmap(hbmp, size);
        saveThumbnail->Call(bmp);
    }
    // delay deleting because ~ChmThumbnailTask() deletes HtmlWindow
    // and we're currently processing HtmlWindow messages
    // TODO: it's possible we still have timing issue
    auto fn = MkFunc0<ChmThumbnailTask>(SafeDeleteChmThumbnailTask, this);
    uitask::Post(fn, "SafeDeleteChmThumbnailTask");
}

void ChmThumbnailTask::OnLButtonDown() {}

void ChmThumbnailTask::DownloadData(Str, Str) {}

static void CreateChmThumbnail(Str path, const Size& size, const OnBitmapRendered* saveThumbnail) {
    // doc and window will be destroyed by the callback once it's invoked
    ChmFile* doc = ChmFile::CreateFromFile(path);
    if (!doc) {
        return;
    }

    // We render twice the size of thumbnail and scale it down
    int dx = size.dx * 2 + GetSystemMetrics(SM_CXVSCROLL);
    int dy = size.dy * 2 + GetSystemMetrics(SM_CYHSCROLL);
    // reusing WC_STATIC. I don't think exact class matters (WndProc
    // will be taken over by HtmlWindow anyway) but it can't be nullptr.
    HWND hwnd =
        CreateWindowExW(0, WC_STATIC, L"BrowserCapture", WS_POPUP, 0, 0, dx, dy, nullptr, nullptr, nullptr, nullptr);
    if (!hwnd) {
        delete doc;
        return;
    }
#if 0 // when debugging set to 1 to see the window
    ShowWindow(hwnd, SW_SHOW);
#endif

    ChmThumbnailTask* thumbnailTask = new ChmThumbnailTask(doc, hwnd, size, saveThumbnail);
    HtmlWindow* hw = HtmlWindow::Create(hwnd, thumbnailTask);
    if (!hw) {
        delete thumbnailTask;
        return;
    }
    // is deleted in ChmThumbnailTask::OnDocumentComplete
    thumbnailTask->StartCreateThumbnail(hw);
}

// Create a thumbnail of chm document by loading it again and rendering
// its first page to a hwnd specially created for it.
void ChmModel::CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) {
    CreateChmThumbnail(fileName, size, saveThumbnail);
}

bool ChmModel::IsSupportedFileType(Kind kind) {
    return ChmFile::IsSupportedFileType(kind);
}

ChmModel* ChmModel::Create(Str fileName, DocControllerCallback* cb) {
    ChmModel* cm = new ChmModel(cb);
    if (!cm->Load(fileName)) {
        delete cm;
        return nullptr;
    }
    return cm;
}
