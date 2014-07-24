/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ChmModel.h"

#include "AppPrefs.h" // for gGlobalPrefs
#include "ChmDoc.h"
#include "Dict.h"
#include "HtmlWindow.h"
#include "UITask.h"

static bool IsExternalUrl(const WCHAR *url)
{
    return str::StartsWithI(url, L"http://") ||
           str::StartsWithI(url, L"https://") ||
           str::StartsWithI(url, L"mailto:");
}

class ChmTocItem : public DocTocItem, public PageDestination {
public:
    const WCHAR *url; // owned by ChmModel::poolAllocator or ChmNamedDest::myUrl

    ChmTocItem(const WCHAR *title, int pageNo, const WCHAR *url) :
        DocTocItem((WCHAR *)title, pageNo), url(url) { }
    virtual ~ChmTocItem() {
        // prevent title from being freed
        title = NULL;
    }

    virtual PageDestination *GetLink() { return url ? this : NULL; }
    virtual PageDestType GetDestType() const {
        return !url ? Dest_None : IsExternalUrl(url) ? Dest_LaunchURL : Dest_ScrollTo;
    }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const {
        return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    }
    virtual WCHAR *GetDestValue() const {
        return url && IsExternalUrl(url) ? str::Dup(url) : NULL;
    }
    virtual WCHAR *GetDestName() const {
        return url && !IsExternalUrl(url) ? str::Dup(url) : NULL;
    }
};

class ChmNamedDest : public ChmTocItem {
    ScopedMem<WCHAR> myUrl;

public:
    ChmNamedDest(const WCHAR *url, int pageNo) :
        ChmTocItem(NULL, pageNo, NULL), myUrl(str::Dup(url)) {
        this->url = this->title = myUrl;
    }
    virtual ~ChmNamedDest() { }
};

class HtmlWindowHandler : public HtmlWindowCallback {
    ChmModel *cm;

public:
    HtmlWindowHandler(ChmModel *cm) : cm(cm) { }
    virtual ~HtmlWindowHandler() { }

    virtual bool OnBeforeNavigate(const WCHAR *url, bool newWindow) { return cm->OnBeforeNavigate(url, newWindow); }
    virtual void OnDocumentComplete(const WCHAR *url) { cm->OnDocumentComplete(url); }
    virtual void OnLButtonDown() { cm->OnLButtonDown(); }
    virtual const unsigned char *GetDataForUrl(const WCHAR *url, size_t *len) { return cm->GetDataForUrl(url, len); }
    virtual void DownloadData(const WCHAR *url, const unsigned char *data, size_t len) { cm->DownloadData(url, data, len); }
};

struct ChmTocTraceItem {
    const WCHAR *title; // owned by ChmModel::poolAllocator
    const WCHAR *url;   // owned by ChmModel::poolAllocator
    int level;
    int pageNo;

    explicit ChmTocTraceItem(const WCHAR *title=NULL, const WCHAR *url=NULL, int level=0, int pageNo=0) :
        title(title), url(url), level(level), pageNo(pageNo) { }
};

ChmModel::ChmModel(ControllerCallback *cb) : Controller(cb),
    doc(NULL), htmlWindow(NULL), htmlWindowCb(NULL), tocTrace(NULL),
    currentPageNo(1), initZoom(INVALID_ZOOM)
{
    InitializeCriticalSection(&docAccess);
}

ChmModel::~ChmModel()
{
    EnterCriticalSection(&docAccess);
    // TODO: deleting htmlWindow seems to spin a modal loop which
    //       can lead to WM_PAINT being dispatched for the parent
    //       hwnd and then crashing in SumatraPDF.cpp's DrawDocument
    delete htmlWindow;
    delete htmlWindowCb;
    delete doc;
    delete tocTrace;
    DeleteVecMembers(urlDataCache);
    LeaveCriticalSection(&docAccess);
    DeleteCriticalSection(&docAccess);
}

int ChmModel::PageCount() const
{
    return (int)pages.Count();
}

WCHAR *ChmModel::GetProperty(DocumentProperty prop)
{
    return doc->GetProperty(prop);
}

bool ChmModel::SetParentHwnd(HWND hwnd)
{
    CrashIf(htmlWindow || htmlWindowCb);
    htmlWindowCb = new HtmlWindowHandler(this);
    htmlWindow = HtmlWindow::Create(hwnd, htmlWindowCb);
    if (!htmlWindow) {
        delete htmlWindowCb;
        htmlWindowCb = NULL;
        return false;
    }
    return true;
}

void ChmModel::RemoveParentHwnd()
{
    delete htmlWindow;
    htmlWindow = NULL;
    delete htmlWindowCb;
    htmlWindowCb = NULL;
}

void ChmModel::PrintCurrentPage(bool showUI)
{
    if (htmlWindow)
        htmlWindow->PrintCurrentPage(showUI);
}

void ChmModel::FindInCurrentPage()
{
    if (htmlWindow)
        htmlWindow->FindInCurrentPage();
}

void ChmModel::SelectAll()
{
    if (htmlWindow)
        htmlWindow->SelectAll();
}

void ChmModel::CopySelection()
{
    if (htmlWindow)
        htmlWindow->CopySelection();
}

LRESULT ChmModel::PassUIMsg(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!htmlWindow)
        return 0;
    return htmlWindow->SendMsg(msg, wParam, lParam);
}

void ChmModel::DisplayPage(const WCHAR *pageUrl)
{
    if (IsExternalUrl(pageUrl)) {
        // open external links in an external browser
        // (same as for PDF, XPS, etc. documents)
        if (cb) {
            ChmTocItem item(NULL, 0, pageUrl);
            cb->GotoLink(&item);
        }
        return;
    }

    int pageNo = pages.Find(ScopedMem<WCHAR>(url::GetFullPath(pageUrl))) + 1;
    if (pageNo)
        currentPageNo = pageNo;

    // This is a hack that seems to be needed for some chm files where
    // url starts with "..\" even though it's not accepted by ie as
    // a correct its: url. There's a possibility it breaks some other
    // chm files (I don't know such cases, though).
    // A more robust solution would try to match with the actual
    // names of files inside chm package.
    if (str::StartsWith(pageUrl, L"..\\"))
        pageUrl += 3;

    if (str::StartsWith(pageUrl, L"/"))
        pageUrl++;

    CrashIf(!htmlWindow);
    if (htmlWindow)
        htmlWindow->NavigateToDataUrl(pageUrl);
}

void ChmModel::ScrollToLink(PageDestination *link)
{
    CrashIf(link->GetDestType() != Dest_ScrollTo);
    ScopedMem<WCHAR> url(link->GetDestName());
    if (url)
        DisplayPage(url);
}

bool ChmModel::CanNavigate(int dir) const
{
    if (!htmlWindow)
        return false;
    if (dir < 0)
        return htmlWindow->canGoBack;
    return htmlWindow->canGoForward;
}

void ChmModel::Navigate(int dir)
{
    if (!htmlWindow)
        return;
    if (dir < 0) {
        for (; dir < 0 && CanNavigate(dir); dir++)
            htmlWindow->GoBack();
    } else {
        for (; dir > 0 && CanNavigate(dir); dir--)
            htmlWindow->GoForward();
    }
}

void ChmModel::SetZoomVirtual(float zoom, PointI *fixPt)
{
    if (zoom <= 0 || !IsValidZoom(zoom))
        zoom = 100.0f;
    ZoomTo(zoom);
    initZoom = zoom;
}

void ChmModel::ZoomTo(float zoomLevel)
{
    if (htmlWindow)
        htmlWindow->SetZoomPercent((int)zoomLevel);
}

float ChmModel::GetZoomVirtual() const
{
    if (!htmlWindow)
        return 100;
    return (float)htmlWindow->GetZoomPercent();
}

class ChmTocBuilder : public EbookTocVisitor {
    ChmDoc *doc;

    WStrList *pages;
    Vec<ChmTocTraceItem> *tocTrace;
    Allocator *allocator;
    // TODO: could use dict::MapWStrToInt instead of StrList in the caller as well
    dict::MapWStrToInt urlsSet;

    // We fake page numbers by doing a depth-first traversal of
    // toc tree and considering each unique html page in toc tree
    // as a page
    int CreatePageNoForURL(const WCHAR *url) {
        if (!url || IsExternalUrl(url))
            return 0;

        ScopedMem<WCHAR> plainUrl(url::GetFullPath(url));
        int pageNo = (int)pages->Count() + 1;
        bool inserted = urlsSet.Insert(plainUrl, pageNo, &pageNo);
        if (inserted) {
            pages->Append(plainUrl.StealData());
            CrashIf((size_t)pageNo != pages->Count());
        } else {
            CrashIf((size_t)pageNo == pages->Count() + 1);
        }
        return pageNo;
    }

public:
    ChmTocBuilder(ChmDoc *doc, WStrList *pages, Vec<ChmTocTraceItem> *tocTrace, Allocator *allocator) :
        doc(doc), pages(pages), tocTrace(tocTrace), allocator(allocator)
        {
            for (int i = 0; i < (int)pages->Count(); i++) {
                const WCHAR *url = pages->At(i);
                bool inserted = urlsSet.Insert(url, i + 1, NULL);
                CrashIf(!inserted);
            }
        }

    virtual void Visit(const WCHAR *name, const WCHAR *url, int level) {
        int pageNo = CreatePageNoForURL(url);
        name = Allocator::StrDup(allocator, name);
        url = Allocator::StrDup(allocator, url);
        tocTrace->Append(ChmTocTraceItem(name, url, level, pageNo));
    }
};

bool ChmModel::Load(const WCHAR *fileName)
{
    this->fileName.Set(str::Dup(fileName));
    doc = ChmDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    // always make the document's homepage page 1
    pages.Append(str::conv::FromAnsi(doc->GetHomePath()));

    // parse the ToC here, since page numbering depends on it
    tocTrace = new Vec<ChmTocTraceItem>();
    ChmTocBuilder tmpTocBuilder(doc, &pages, tocTrace, &poolAlloc);
    doc->ParseToc(&tmpTocBuilder);
    CrashIf(pages.Count() == 0);
    return pages.Count() > 0;
}

class ChmCacheEntry {
public:
    const WCHAR *url; // owned by ChmModel::poolAllocator
    unsigned char *data;
    size_t size;

    explicit ChmCacheEntry(const WCHAR *url) : url(url), data(NULL), size(0) { }
    ~ChmCacheEntry() { free(data); }
};

ChmCacheEntry *ChmModel::FindDataForUrl(const WCHAR *url)
{
    for (size_t i = 0; i < urlDataCache.Count(); i++) {
        ChmCacheEntry *e = urlDataCache.At(i);
        if (str::Eq(url, e->url))
            return e;
    }
    return NULL;
}

// Called after html document has been loaded.
// Sync the state of the ui with the page (show
// the right page number, select the right item in toc tree)
void ChmModel::OnDocumentComplete(const WCHAR *url)
{
    if (!url || IsBlankUrl(url))
        return;
    if (*url == '/')
        ++url;
    int pageNo = pages.Find(ScopedMem<WCHAR>(url::GetFullPath(url))) + 1;
    if (pageNo) {
        currentPageNo = pageNo;
        // TODO: setting zoom before the first page is loaded seems not to work
        // (might be a regression from between r4593 and r4629)
        if (IsValidZoom(initZoom)) {
            SetZoomVirtual(initZoom);
            initZoom = INVALID_ZOOM;
        }
        if (cb)
            cb->PageNoChanged(pageNo);
    }
}

// Called before we start loading html for a given url. Will block
// loading if returns false.
bool ChmModel::OnBeforeNavigate(const WCHAR *url, bool newWindow)
{
    // ensure that JavaScript doesn't keep the focus
    // in the HtmlWindow when a new page is loaded
    if (cb)
        cb->FocusFrame(false);

    if (newWindow) {
        // don't allow new MSIE windows to be opened
        // instead pass the URL to the system's default browser
        if (url && cb) {
            ChmTocItem item(NULL, 0, url);
            cb->GotoLink(&item);
        }
        return false;
    }
    return true;
}

// Load and cache data for a given url inside CHM file.
const unsigned char *ChmModel::GetDataForUrl(const WCHAR *url, size_t *len)
{
    ScopedCritSec scope(&docAccess);
    ScopedMem<WCHAR> plainUrl(url::GetFullPath(url));
    ChmCacheEntry *e = FindDataForUrl(plainUrl);
    if (!e) {
        e = new ChmCacheEntry(Allocator::StrDup(&poolAlloc, plainUrl));
        ScopedMem<char> urlUtf8(str::conv::ToUtf8(plainUrl));
        e->data = doc->GetData(urlUtf8, &e->size);
        if (!e->data) {
            delete e;
            return NULL;
        }
        urlDataCache.Append(e);
    }
    if (len)
        *len = e->size;
    return e->data;
}

void ChmModel::DownloadData(const WCHAR *url, const unsigned char *data, size_t len)
{
    if (cb)
        cb->SaveDownload(url, data, len);
}

void ChmModel::OnLButtonDown()
{
    if (cb)
        cb->FocusFrame(true);
}

PageDestination *ChmModel::GetNamedDest(const WCHAR *name)
{
    ScopedMem<WCHAR> plainUrl(url::GetFullPath(name));
    int pageNo = pages.Find(plainUrl) + 1;
    if (pageNo > 0)
        return new ChmNamedDest(name, pageNo);
    return NULL;
}

bool ChmModel::HasTocTree() const
{
     return tocTrace->Count() > 0;
}

// Callers delete the ToC tree, so we re-create it from prerecorded
// values (which is faster than re-creating it from html every time)
DocTocItem *ChmModel::GetTocTree()
{
    DocTocItem *root = NULL, **nextChild = &root;
    Vec<DocTocItem *> levels;
    int idCounter = 0;

    for (ChmTocTraceItem *ti = tocTrace->IterStart(); ti; ti = tocTrace->IterNext()) {
        ChmTocItem *item = new ChmTocItem(ti->title, ti->pageNo, ti->url);
        item->id = ++idCounter;
        // append the item at the correct level
        CrashIf(ti->level < 1);
        if ((size_t)ti->level <= levels.Count()) {
            levels.RemoveAt(ti->level, levels.Count() - ti->level);
            levels.Last()->AddSibling(item);
        }
        else {
            (*nextChild) = item;
            levels.Append(item);
        }
        nextChild = &item->child;
    }

    if (root)
        root->OpenSingleNode();
    return root;
}

// adapted from DisplayModel::NextZoomStep
float ChmModel::GetNextZoomStep(float towardsLevel) const
{
    float currZoom = GetZoomVirtual();

    if (gGlobalPrefs->zoomIncrement > 0) {
        if (currZoom < towardsLevel)
            return min(currZoom * (gGlobalPrefs->zoomIncrement / 100 + 1), towardsLevel);
        if (currZoom > towardsLevel)
            return max(currZoom / (gGlobalPrefs->zoomIncrement / 100 + 1), towardsLevel);
        return currZoom;
    }

    Vec<float> *zoomLevels = gGlobalPrefs->zoomLevels;
    CrashIf(zoomLevels->Count() != 0 && (zoomLevels->At(0) < ZOOM_MIN || zoomLevels->Last() > ZOOM_MAX));
    CrashIf(zoomLevels->Count() != 0 && zoomLevels->At(0) > zoomLevels->Last());

    const float FUZZ = 0.01f;
    float newZoom = towardsLevel;
    if (currZoom < towardsLevel) {
        for (size_t i = 0; i < zoomLevels->Count(); i++) {
            if (zoomLevels->At(i) - FUZZ > currZoom) {
                newZoom = zoomLevels->At(i);
                break;
            }
        }
    }
    else if (currZoom > towardsLevel) {
        for (size_t i = zoomLevels->Count(); i > 0; i--) {
            if (zoomLevels->At(i - 1) + FUZZ < currZoom) {
                newZoom = zoomLevels->At(i - 1);
                break;
            }
        }
    }

    return newZoom;
}

void ChmModel::UpdateDisplayState(DisplayState *ds)
{
    if (!ds->filePath || !str::EqI(ds->filePath, fileName))
        str::ReplacePtr(&ds->filePath, fileName);

    ds->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    str::ReplacePtr(&ds->displayMode, prefs::conv::FromDisplayMode(GetDisplayMode()));
    prefs::conv::FromZoom(&ds->zoom, GetZoomVirtual(), ds);

    ds->pageNo = CurrentPageNo();
    ds->scrollPos = PointI();
}

class ChmThumbnailTask : public HtmlWindowCallback, public UITask
{
    ChmDoc *doc;
    HWND hwnd;
    HtmlWindow *hw;
    SizeI size;
    ThumbnailCallback *tnCb;
    ScopedMem<WCHAR> homeUrl;
    Vec<unsigned char *> data;
    CRITICAL_SECTION docAccess;

public:
    ChmThumbnailTask(ChmDoc *doc, HWND hwnd, SizeI size, ThumbnailCallback *tnCb) :
        doc(doc), hwnd(hwnd), hw(NULL), size(size), tnCb(tnCb) {
        InitializeCriticalSection(&docAccess);
    }

    ~ChmThumbnailTask() {
        EnterCriticalSection(&docAccess);
        delete hw;
        DestroyWindow(hwnd);
        delete doc;
        delete tnCb;
        FreeVecMembers(data);
        LeaveCriticalSection(&docAccess);
        DeleteCriticalSection(&docAccess);
    }

    void CreateThumbnail(HtmlWindow *hw) {
        this->hw = hw;
        homeUrl.Set(str::conv::FromAnsi(doc->GetHomePath()));
        if (*homeUrl == '/')
            homeUrl.Set(str::Dup(homeUrl + 1));
        hw->NavigateToDataUrl(homeUrl);
    }

    virtual bool OnBeforeNavigate(const WCHAR *url, bool newWindow) { return !newWindow; }
    virtual void OnDocumentComplete(const WCHAR *url) {
        if (url && *url == '/')
            url++;
        if (str::Eq(url, homeUrl)) {
            RectI area(0, 0, size.dx * 2, size.dy * 2);
            HBITMAP hbmp = hw->TakeScreenshot(area, size);
            if (hbmp) {
                RenderedBitmap *bmp = new RenderedBitmap(hbmp, size);
                tnCb->SaveThumbnail(bmp);
                tnCb = NULL;
            }
            uitask::Post(this);
        }
    }
    virtual void OnLButtonDown() { }
    virtual const unsigned char *GetDataForUrl(const WCHAR *url, size_t *len) {
        ScopedCritSec scope(&docAccess);
        ScopedMem<WCHAR> plainUrl(url::GetFullPath(url));
        ScopedMem<char> urlUtf8(str::conv::ToUtf8(plainUrl));
        data.Append(doc->GetData(urlUtf8, len));
        return data.Last();
    }
    virtual void DownloadData(const WCHAR *url, const unsigned char *data, size_t len) { }

    virtual void Execute() { }
};

// Create a thumbnail of chm document by loading it again and rendering
// its first page to a hwnd specially created for it.
void ChmModel::CreateThumbnail(SizeI size, ThumbnailCallback *tnCb)
{
    // doc and window will be destroyed by the callback once it's invoked
    ChmDoc *doc = ChmDoc::CreateFromFile(fileName);
    if (!doc) {
        delete tnCb;
    }

    // We render twice the size of thumbnail and scale it down
    int winDx = size.dx * 2 + GetSystemMetrics(SM_CXVSCROLL);
    int winDy = size.dy * 2 + GetSystemMetrics(SM_CYHSCROLL);
    // reusing WC_STATIC. I don't think exact class matters (WndProc
    // will be taken over by HtmlWindow anyway) but it can't be NULL.
    HWND hwnd = CreateWindow(WC_STATIC, L"BrowserCapture", WS_POPUP,
                             0, 0, winDx, winDy, NULL, NULL, NULL, NULL);
    if (!hwnd) {
        delete tnCb;
        delete doc;
        return;
    }
#if 0 // when debugging set to 1 to see the window
    ShowWindow(hwnd, SW_SHOW);
#endif

    ChmThumbnailTask *thumbCb = new ChmThumbnailTask(doc, hwnd, size, tnCb);
    HtmlWindow *hw = HtmlWindow::Create(hwnd, thumbCb);
    if (!hw) {
        delete thumbCb;
        return;
    }
    thumbCb->CreateThumbnail(hw);
}

bool ChmModel::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return ChmDoc::IsSupportedFile(fileName, sniff);
}

ChmModel *ChmModel::Create(const WCHAR *fileName, ControllerCallback *cb)
{
    ChmModel *cm = new ChmModel(cb);
    if (!cm->Load(fileName)) {
        delete cm;
        return NULL;
    }
    return cm;
}
