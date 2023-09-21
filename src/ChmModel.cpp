/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/Dict.h"
#include "utils/HtmlWindow.h"
#include "utils/UITask.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"

static TocItem* NewChmTocItem(TocItem* parent, const char* title, int pageNo, const char* url) {
    auto res = new TocItem(parent, title, pageNo);
    if (!url) {
        return res;
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
    CrashIf(!dest->kind);

    dest->rect = RectF(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    res->dest = dest;
    return res;
}

static TocItem* NewChmNamedDest(const char* url, int pageNo) {
    auto res = NewChmTocItem(nullptr, url, pageNo, nullptr);
    return res;
}

class HtmlWindowHandler : public HtmlWindowCallback {
    ChmModel* cm;

  public:
    explicit HtmlWindowHandler(ChmModel* cm) : cm(cm) {
    }
    ~HtmlWindowHandler() override = default;

    bool OnBeforeNavigate(const char* url, bool newWindow) override {
        return cm->OnBeforeNavigate(url, newWindow);
    }
    void OnDocumentComplete(const char* url) override {
        cm->OnDocumentComplete(url);
    }
    void OnLButtonDown() override {
        cm->OnLButtonDown();
    }
    ByteSlice GetDataForUrl(const char* url) override {
        return cm->GetDataForUrl(url);
    }
    void DownloadData(const char* url, const ByteSlice& data) override {
        cm->DownloadData(url, data);
    }
};

struct ChmTocTraceItem {
    const char* title = nullptr; // owned by ChmModel::poolAllocator
    const char* url = nullptr;   // owned by ChmModel::poolAllocator
    int level = 0;
    int pageNo = 0;
};

ChmModel::ChmModel(DocControllerCallback* cb) : DocController(cb) {
    InitializeCriticalSection(&docAccess);
}

ChmModel::~ChmModel() {
    EnterCriticalSection(&docAccess);
    // TODO: deleting htmlWindow seems to spin a modal loop which
    //       can lead to WM_PAINT being dispatched for the parent
    //       hwnd and then crashing in SumatraPDF.cpp's DrawDocument
    delete htmlWindow;
    delete htmlWindowCb;
    delete doc;
    delete tocTrace;
    delete tocTree;
    DeleteVecMembers(urlDataCache);
    LeaveCriticalSection(&docAccess);
    DeleteCriticalSection(&docAccess);
}

const char* ChmModel::GetFilePath() const {
    return fileName;
}

const char* ChmModel::GetDefaultFileExt() const {
    return ".chm";
}

int ChmModel::PageCount() const {
    return (int)pages.size();
}

char* ChmModel::GetProperty(DocumentProperty prop) {
    return doc->GetProperty(prop);
}

int ChmModel::CurrentPageNo() const {
    return currentPageNo;
}

void ChmModel::GoToPage(int pageNo, bool) {
    ReportIf(!ValidPageNo(pageNo));
    if (!ValidPageNo(pageNo)) {
        return;
    }
    DisplayPage(pages.at(pageNo - 1));
}

bool ChmModel::SetParentHwnd(HWND hwnd) {
    CrashIf(htmlWindow || htmlWindowCb);
    htmlWindowCb = new HtmlWindowHandler(this);
    htmlWindow = HtmlWindow::Create(hwnd, htmlWindowCb);
    if (!htmlWindow) {
        delete htmlWindowCb;
        htmlWindowCb = nullptr;
        return false;
    }
    return true;
}

void ChmModel::RemoveParentHwnd() {
    delete htmlWindow;
    htmlWindow = nullptr;
    delete htmlWindowCb;
    htmlWindowCb = nullptr;
}

void ChmModel::PrintCurrentPage(bool showUI) const {
    if (htmlWindow) {
        htmlWindow->PrintCurrentPage(showUI);
    }
}

void ChmModel::FindInCurrentPage() const {
    if (htmlWindow) {
        htmlWindow->FindInCurrentPage();
    }
}

void ChmModel::SelectAll() const {
    if (htmlWindow) {
        htmlWindow->SelectAll();
    }
}

void ChmModel::CopySelection() const {
    if (htmlWindow) {
        htmlWindow->CopySelection();
    }
}

LRESULT ChmModel::PassUIMsg(UINT msg, WPARAM wp, LPARAM lp) const {
    if (!htmlWindow) {
        return 0;
    }
    return htmlWindow->SendMsg(msg, wp, lp);
}

void ChmModel::DisplayPage(const char* pageUrl) {
    if (IsExternalUrl(pageUrl)) {
        // open external links in an external browser
        // (same as for PDF, XPS, etc. documents)
        if (cb) {
            // TODO: optimize, create just destination
            auto item = NewChmTocItem(nullptr, nullptr, 0, pageUrl);
            cb->GotoLink(item->dest);
            delete item;
        }
        return;
    }

    int pageNo = pages.Find(url::GetFullPathTemp(pageUrl)) + 1;
    if (pageNo) {
        currentPageNo = pageNo;
    }

    // This is a hack that seems to be needed for some chm files where
    // url starts with "..\" even though it's not accepted by ie as
    // a correct its: url. There's a possibility it breaks some other
    // chm files (I don't know such cases, though).
    // A more robust solution would try to match with the actual
    // names of files inside chm package.
    if (str::StartsWith(pageUrl, "..\\")) {
        pageUrl += 3;
    }

    if (str::StartsWith(pageUrl, "/")) {
        pageUrl++;
    }

    CrashIf(!htmlWindow);
    if (htmlWindow) {
        htmlWindow->NavigateToDataUrl(pageUrl);
    }
}

void ChmModel::ScrollTo(int, RectF, float) {
    CrashIf(true);
}

bool ChmModel::HandleLink(IPageDestination* link, ILinkHandler*) {
    CrashIf(link->GetKind() != kindDestinationScrollTo);
    char* url = link->GetName();
    if (url) {
        DisplayPage(url);
    }
    return false;
}

bool ChmModel::CanNavigate(int dir) const {
    if (!htmlWindow) {
        return false;
    }
    if (dir < 0) {
        return htmlWindow->canGoBack;
    }
    return htmlWindow->canGoForward;
}

void ChmModel::Navigate(int dir) {
    if (!htmlWindow) {
        return;
    }

    if (dir < 0) {
        for (; dir < 0 && CanNavigate(dir); dir++) {
            htmlWindow->GoBack();
        }
    } else {
        for (; dir > 0 && CanNavigate(dir); dir--) {
            htmlWindow->GoForward();
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
    initZoom = zoom;
}

void ChmModel::ZoomTo(float zoomLevel) const {
    if (htmlWindow) {
        htmlWindow->SetZoomPercent((int)zoomLevel);
    }
}

float ChmModel::GetZoomVirtual(bool) const {
    if (!htmlWindow) {
        return 100;
    }
    return (float)htmlWindow->GetZoomPercent();
}

class ChmTocBuilder : public EbookTocVisitor {
    ChmFile* doc = nullptr;

    StrVec* pages = nullptr;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;
    Allocator* allocator = nullptr;
    // TODO: could use dict::MapWStrToInt instead of StrList in the caller as well
    dict::MapStrToInt urlsSet;

    // We fake page numbers by doing a depth-first traversal of
    // toc tree and considering each unique html page in toc tree
    // as a page
    int CreatePageNoForURL(const char* url) {
        if (!url || IsExternalUrl(url)) {
            return 0;
        }

        char* plainUrl = url::GetFullPathTemp(url);
        int pageNo = (int)pages->size() + 1;
        bool inserted = urlsSet.Insert(plainUrl, pageNo, &pageNo);
        if (inserted) {
            pages->Append(plainUrl);
            CrashIf((size_t)pageNo != pages->size());
        } else {
            CrashIf((size_t)pageNo == pages->size() + 1);
        }
        return pageNo;
    }

  public:
    ChmTocBuilder(ChmFile* doc, StrVec* pages, Vec<ChmTocTraceItem>* tocTrace, Allocator* allocator) {
        this->doc = doc;
        this->pages = pages;
        this->tocTrace = tocTrace;
        this->allocator = allocator;
        int n = (int)pages->size();
        for (int i = 0; i < n; i++) {
            const char* url = pages->at(i);
            bool inserted = urlsSet.Insert(url, i + 1, nullptr);
            CrashIf(!inserted);
        }
    }

    void Visit(const char* name, const char* url, int level) override {
        name = str::Dup(allocator, name, (size_t)-1);
        url = str::Dup(allocator, url, (size_t)-1);
        int pageNo = CreatePageNoForURL(url);
        auto item = ChmTocTraceItem{name, url, level, pageNo};
        tocTrace->Append(item);
    }
};

bool ChmModel::Load(const char* fileName) {
    this->fileName.SetCopy(fileName);
    doc = ChmFile::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    // always make the document's homepage page 1
    char* page = strconv::AnsiToUtf8(doc->GetHomePath());
    pages.Append(page);
    str::Free(page);

    // parse the ToC here, since page numbering depends on it
    tocTrace = new Vec<ChmTocTraceItem>();
    ChmTocBuilder tmpTocBuilder(doc, &pages, tocTrace, &poolAlloc);
    doc->ParseToc(&tmpTocBuilder);
    CrashIf(pages.size() == 0);
    return pages.size() > 0;
}

struct ChmCacheEntry {
    // owned by ChmModel::poolAllocator
    const char* url = nullptr;
    ByteSlice data;

    explicit ChmCacheEntry(const char* url);
    ~ChmCacheEntry() {
        data.Free();
    };
};

ChmCacheEntry::ChmCacheEntry(const char* url) {
    this->url = url;
}

ChmCacheEntry* ChmModel::FindDataForUrl(const char* url) const {
    size_t n = urlDataCache.size();
    for (size_t i = 0; i < n; i++) {
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
void ChmModel::OnDocumentComplete(const char* url) {
    if (!url || IsBlankUrl(url)) {
        return;
    }
    if (*url == '/') {
        ++url;
    }
    char* toFind = url::GetFullPathTemp(url);
    int pageNo = pages.Find(toFind) + 1;
    if (!pageNo) {
        return;
    }
    currentPageNo = pageNo;
    // TODO: setting zoom before the first page is loaded seems not to work
    // (might be a regression from between r4593 and r4629)
    if (IsValidZoom(initZoom)) {
        SetZoomVirtual(initZoom, nullptr);
        initZoom = kInvalidZoom;
    }
    if (cb) {
        cb->PageNoChanged(this, pageNo);
    }
}

// Called before we start loading html for a given url. Will block
// loading if returns false.
bool ChmModel::OnBeforeNavigate(const char* url, bool newWindow) {
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
        auto item = NewChmTocItem(nullptr, nullptr, 0, url);
        cb->GotoLink(item->dest);
        delete item;
    }
    return false;
}

// Load and cache data for a given url inside CHM file.
ByteSlice ChmModel::GetDataForUrl(const char* url) {
    ScopedCritSec scope(&docAccess);
    char* plainUrl = url::GetFullPathTemp(url);
    ChmCacheEntry* e = FindDataForUrl(plainUrl);
    if (!e) {
        char* s = str::Dup(&poolAlloc, plainUrl);
        e = new ChmCacheEntry(s);
        e->data = doc->GetData(plainUrl);
        if (e->data.empty()) {
            delete e;
            return {};
        }
        urlDataCache.Append(e);
    }
    return e->data;
}

void ChmModel::DownloadData(const char* url, const ByteSlice& data) {
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
IPageDestination* ChmModel::GetNamedDest(const char* name) {
    char* plainUrl = url::GetFullPathTemp(name);
    char* url = str::DupTemp(plainUrl);
    if (!doc->HasData(url)) {
        unsigned int topicID;
        if (str::Parse(name, "%u%$", &topicID)) {
            char* s = doc->ResolveTopicID(topicID);
            url = str::DupTemp(s);
            str::Free(s);
            if (url && doc->HasData(url)) {
                plainUrl = url;
                name = plainUrl;
            } else {
                url = nullptr;
            }
        } else {
            url = nullptr;
        }
    }
    int pageNo = pages.Find(plainUrl) + 1;
    if (!pageNo && !str::IsEmpty(url)) {
        // some documents use redirection URLs which aren't listed in the ToC
        // return pageNo=1 for these, as HandleLink will ignore that anyway
        // but LinkHandler::ScrollTo doesn't
        pageNo = 1;
    }
    if (pageNo > 0) {
        // TODO: make a function just for constructing a destination
        auto tmp = NewChmNamedDest(name, pageNo);
        auto res = tmp->dest;
        tmp->dest = nullptr;
        delete tmp;
        return res;
    }
    return nullptr;
}

TocTree* ChmModel::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    if (tocTrace->size() == 0) {
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
        CrashIf(ti.level < 1);
        if ((size_t)ti.level <= levels.size()) {
            levels.RemoveAt(ti.level, levels.size() - ti.level);
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

    if (gGlobalPrefs->zoomIncrement > 0) {
        float z1 = currZoom * (gGlobalPrefs->zoomIncrement / 100 + 1);
        if (currZoom < towardsLevel) {
            return std::min(z1, towardsLevel);
        }
        if (currZoom > towardsLevel) {
            return std::max(z1, towardsLevel);
        }
        return currZoom;
    }

    Vec<float>* zoomLevels = gGlobalPrefs->zoomLevels;
    CrashIf(zoomLevels->size() != 0 && (zoomLevels->at(0) < kZoomMin || zoomLevels->Last() > kZoomMax));
    CrashIf(zoomLevels->size() != 0 && zoomLevels->at(0) > zoomLevels->Last());

    const float FUZZ = 0.01f;
    float newZoom = towardsLevel;
    if (currZoom < towardsLevel) {
        for (size_t i = 0; i < zoomLevels->size(); i++) {
            if (zoomLevels->at(i) - FUZZ > currZoom) {
                newZoom = zoomLevels->at(i);
                break;
            }
        }
    } else if (currZoom > towardsLevel) {
        for (size_t i = zoomLevels->size(); i > 0; i--) {
            if (zoomLevels->at(i - 1) + FUZZ < currZoom) {
                newZoom = zoomLevels->at(i - 1);
                break;
            }
        }
    }

    return newZoom;
}

void ChmModel::GetDisplayState(FileState* fs) {
    char* fileNameA = fileName;
    if (!fs->filePath || !str::EqI(fs->filePath, fileNameA)) {
        SetFileStatePath(fs, fileNameA);
    }

    fs->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    str::ReplaceWithCopy(&fs->displayMode, DisplayModeToString(GetDisplayMode()));
    ZoomToString(&fs->zoom, GetZoomVirtual(), fs);

    fs->pageNo = CurrentPageNo();
    fs->scrollPos = PointF();
}

class ChmThumbnailTask : public HtmlWindowCallback {
    ChmFile* doc = nullptr;
    HWND hwnd = nullptr;
    HtmlWindow* hw = nullptr;
    Size size;
    onBitmapRenderedCb saveThumbnail;
    AutoFreeStr homeUrl;
    Vec<ByteSlice> data;
    CRITICAL_SECTION docAccess;

  public:
    ChmThumbnailTask(ChmFile* doc, HWND hwnd, Size size, const onBitmapRenderedCb& saveThumbnail) {
        this->doc = doc;
        this->hwnd = hwnd;
        this->size = size;
        this->saveThumbnail = saveThumbnail;
        InitializeCriticalSection(&docAccess);
    }

    ~ChmThumbnailTask() override {
        EnterCriticalSection(&docAccess);
        delete hw;
        DestroyWindow(hwnd);
        delete doc;
        for (auto&& d : data) {
            str::Free(d.data());
        }
        LeaveCriticalSection(&docAccess);
        DeleteCriticalSection(&docAccess);
    }

    void CreateThumbnail(HtmlWindow* hw) {
        this->hw = hw;
        homeUrl.Set(strconv::AnsiToUtf8(doc->GetHomePath()));
        if (*homeUrl == '/') {
            homeUrl.SetCopy(homeUrl + 1);
        }
        hw->NavigateToDataUrl(homeUrl);
    }

    bool OnBeforeNavigate(const char*, bool newWindow) override {
        return !newWindow;
    }

    void OnDocumentComplete(const char* url) override {
        if (url && *url == '/') {
            url++;
        }
        if (str::Eq(url, homeUrl)) {
            Rect area(0, 0, size.dx * 2, size.dy * 2);
            HBITMAP hbmp = hw->TakeScreenshot(area, size);
            if (hbmp) {
                RenderedBitmap* bmp = new RenderedBitmap(hbmp, size);
                saveThumbnail(bmp);
            }
            // TODO: why is destruction on the UI thread necessary?
            uitask::Post([=] { delete this; });
        }
    }

    void OnLButtonDown() override {
    }

    ByteSlice GetDataForUrl(const char* url) override {
        ScopedCritSec scope(&docAccess);
        char* plainUrl = url::GetFullPathTemp(url);
        auto d = doc->GetData(plainUrl);
        data.Append(d);
        return d;
    }

    void DownloadData(const char*, const ByteSlice&) override {
    }
};

static void CreateChmThumbnail(const char* path, const Size& size, const onBitmapRenderedCb& saveThumbnail) {
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
    thumbnailTask->CreateThumbnail(hw);
}

// Create a thumbnail of chm document by loading it again and rendering
// its first page to a hwnd specially created for it.
void ChmModel::CreateThumbnail(Size size, const onBitmapRenderedCb& saveThumbnail) {
    CreateChmThumbnail(fileName, size, saveThumbnail);
}

bool ChmModel::IsSupportedFileType(Kind kind) {
    return ChmFile::IsSupportedFileType(kind);
}

ChmModel* ChmModel::Create(const char* fileName, DocControllerCallback* cb) {
    ChmModel* cm = new ChmModel(cb);
    if (!cm->Load(fileName)) {
        delete cm;
        return nullptr;
    }
    return cm;
}
