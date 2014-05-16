/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ChmEngine.h"
#include "ChmDoc.h"
#include "DebugLog.h"
#include "Dict.h"
#include "FileUtil.h"
#include "HtmlWindow.h"
#include "Timer.h"

static bool IsExternalUrl(const WCHAR *url)
{
    return str::StartsWithI(url, L"http://") ||
           str::StartsWithI(url, L"https://") ||
           str::StartsWithI(url, L"mailto:");
}

struct ChmTocTraceItem {
    const WCHAR *title; // owned by ChmEngineImpl::poolAllocator
    const WCHAR *url;   // owned by ChmEngineImpl::poolAllocator
    int level;
    int pageNo;

    explicit ChmTocTraceItem(const WCHAR *title=NULL, const WCHAR *url=NULL, int level=0, int pageNo=0) :
        title(title), url(url), level(level), pageNo(pageNo) { }
};

class ChmTocItem : public DocTocItem, public PageDestination {
public:
    const WCHAR *url; // owned by ChmEngineImpl::poolAllocator or ChmNamedDest::myUrl

    ChmTocItem(const WCHAR *title, int pageNo, const WCHAR *url) :
        DocTocItem((WCHAR *)title, pageNo), url(url) { }
    virtual ~ChmTocItem() {
        // prevent title from being freed
        title = NULL;
    }

    virtual PageDestination *GetLink() { return this; }
    virtual PageDestType GetDestType() const {
        if (url && IsExternalUrl(url))
            return Dest_LaunchURL;
        return Dest_ScrollTo;
    }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const {
        return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    }
    virtual WCHAR *GetDestValue() const {
        if (url && IsExternalUrl(url))
            return str::Dup(url);
        return NULL;
    }
};

class ChmNamedDest : public ChmTocItem {
    ScopedMem<WCHAR> myUrl;

public:
    ChmNamedDest(const WCHAR *url, int pageNo) :
        ChmTocItem(NULL, pageNo, NULL), myUrl(str::Dup(url)) {
        this->url = myUrl;
    }
};

class ChmCacheEntry {
public:
    const WCHAR *url; // owned by ChmEngineImpl::poolAllocator
    unsigned char *data;
    size_t size;

    explicit ChmCacheEntry(const WCHAR *url) : url(url), data(NULL), size(0) { }
    ~ChmCacheEntry() { free(data); }
};

class ChmEngineImpl : public ChmEngine, public HtmlWindowCallback {
    friend ChmEngine;

public:
    ChmEngineImpl();
    virtual ~ChmEngineImpl();
    virtual ChmEngine *Clone() {
        return CreateFromFile(fileName);
    }

    virtual const WCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return (int)pages.Count(); }
    virtual RectD PageMediabox(int pageNo) { return RectD(); }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL) {
         // TODO: assert(0);
         return NULL;
    }

    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View, AbortCookie **cookie_out=NULL) {
        // TODO: assert(0);
        return false;
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false) {
        return pt;
    }
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false) {
        return rect;
    }
    virtual unsigned char *GetFileData(size_t *cbCount) {
        return (unsigned char *)file::ReadAll(fileName, cbCount);
    }
    virtual bool SaveFileAs(const WCHAR *copyFileName) {
        return CopyFile(fileName, copyFileName, FALSE);
    }

    virtual WCHAR * ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) {
        return NULL;
    }

    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }
    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc->GetProperty(prop); }

    virtual bool SupportsAnnotation(bool forSaving=false) const { return false; }
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list) { }

    virtual const WCHAR *GetDefaultFileExt() const { return L".chm"; }

    virtual Vec<PageElement *> *GetElements(int pageNo) { return NULL; }
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt) { return NULL; }

    virtual bool BenchLoadPage(int pageNo) { return true; }

    virtual PageDestination *GetNamedDest(const WCHAR *name);
    virtual bool HasTocTree() const { return tocTrace.Count() > 0; }
    virtual DocTocItem *GetTocTree();

    virtual bool SetParentHwnd(HWND hwnd);
    virtual void RemoveParentHwnd();
    virtual void DisplayPage(int pageNo) { DisplayPage(pages.At(pageNo - 1)); }
    virtual void SetNavigationCalback(ChmNavigationCallback *cb) { navCb = cb; }

    virtual void GoToDestination(PageDestination *link);
    virtual RenderedBitmap *TakeScreenshot(RectI area, SizeI targetSize);

    virtual void PrintCurrentPage(bool showUI) { if (htmlWindow) htmlWindow->PrintCurrentPage(showUI); }
    virtual void FindInCurrentPage() { if (htmlWindow) htmlWindow->FindInCurrentPage(); }
    virtual bool CanNavigate(int dir);
    virtual void Navigate(int dir);
    virtual void ZoomTo(float zoomLevel);
    virtual void SelectAll() { if (htmlWindow) htmlWindow->SelectAll(); }
    virtual void CopySelection() { if (htmlWindow) htmlWindow->CopySelection(); }
    virtual int CurrentPageNo() const { return currentPageNo; }
    virtual LRESULT PassUIMsg(UINT msg, WPARAM wParam, LPARAM lParam) {
        return htmlWindow ? htmlWindow->SendMsg(msg, wParam, lParam) : 0;
    }

    // from HtmlWindowCallback
    virtual bool OnBeforeNavigate(const WCHAR *url, bool newWindow);
    virtual void OnDocumentComplete(const WCHAR *url);
    virtual void OnLButtonDown() { if (navCb) navCb->FocusFrame(true); }
    virtual const unsigned char *GetDataForUrl(const WCHAR *url, size_t *len);
    virtual void DownloadData(const WCHAR *url, const unsigned char *data, size_t len);

protected:
    WCHAR *fileName;
    ChmDoc *doc;
    Vec<ChmTocTraceItem> tocTrace;

    WStrList pages;
    int currentPageNo;
    HtmlWindow *htmlWindow;
    ChmNavigationCallback *navCb;

    Vec<ChmCacheEntry*> urlDataCache;
    // use a pool allocator for strings that aren't freed until this ChmEngineImpl
    // is deleted (e.g. for titles and URLs for ChmTocItem and ChmCacheEntry)
    PoolAllocator poolAlloc;

    bool Load(const WCHAR *fileName);
    void DisplayPage(const WCHAR *pageUrl);

    ChmCacheEntry *FindDataForUrl(const WCHAR *url);
};

ChmEngineImpl::ChmEngineImpl() : fileName(NULL), doc(NULL),
    htmlWindow(NULL), navCb(NULL), currentPageNo(1)
{
}

ChmEngineImpl::~ChmEngineImpl()
{
    // TODO: deleting htmlWindow seems to spin a modal loop which
    //       can lead to WM_PAINT being dispatched for the parent
    //       hwnd and then crashing in SumatraPDF.cpp's DrawDocument
    delete htmlWindow;
    delete doc;
    free(fileName);
    DeleteVecMembers(urlDataCache);
}

// Called after html document has been loaded.
// Sync the state of the ui with the page (show
// the right page number, select the right item in toc tree)
void ChmEngineImpl::OnDocumentComplete(const WCHAR *url)
{
    if (!url)
        return;
    if (*url == '/')
        ++url;
    int pageNo = pages.Find(ScopedMem<WCHAR>(str::ToPlainUrl(url))) + 1;
    if (pageNo) {
        currentPageNo = pageNo;
        if (navCb)
            navCb->PageNoChanged(pageNo);
    }
}

// Called before we start loading html for a given url. Will block
// loading if returns false.
bool ChmEngineImpl::OnBeforeNavigate(const WCHAR *url, bool newWindow)
{
    // ensure that JavaScript doesn't keep the focus
    // in the HtmlWindow when a new page is loaded
    if (navCb)
        navCb->FocusFrame(false);

    if (newWindow) {
        // don't allow new MSIE windows to be opened
        // instead pass the URL to the system's default browser
        if (url && navCb)
            navCb->LaunchBrowser(url);
        return false;
    }
    return true;
}

bool ChmEngineImpl::SetParentHwnd(HWND hwnd)
{
    CrashIf(htmlWindow);
    htmlWindow = HtmlWindow::Create(hwnd, this);
    return htmlWindow != NULL;
}

void ChmEngineImpl::RemoveParentHwnd()
{
    delete htmlWindow;
    htmlWindow = NULL;
}

void ChmEngineImpl::DisplayPage(const WCHAR *pageUrl)
{
    if (IsExternalUrl(pageUrl)) {
        // open external links in an external browser
        // (same as for PDF, XPS, etc. documents)
        if (navCb)
            navCb->LaunchBrowser(pageUrl);
        return;
    }

    int pageNo = pages.Find(ScopedMem<WCHAR>(str::ToPlainUrl(pageUrl))) + 1;
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

    assert(htmlWindow);
    if (htmlWindow)
        htmlWindow->NavigateToDataUrl(pageUrl);
}

void ChmEngineImpl::GoToDestination(PageDestination *link)
{
    ChmTocItem *item = static_cast<ChmTocItem *>(link);
    if (item && item->url)
        DisplayPage(item->url);
}

RenderedBitmap *ChmEngineImpl::TakeScreenshot(RectI area, SizeI targetSize)
{
    HBITMAP hbmp = htmlWindow->TakeScreenshot(area, targetSize);
    if (!hbmp)
        return NULL;
    return new RenderedBitmap(hbmp, targetSize);
}

bool ChmEngineImpl::CanNavigate(int dir)
{
    if (!htmlWindow)
        return false;
    if (dir < 0)
        return htmlWindow->canGoBack;
    return htmlWindow->canGoForward;
}

void ChmEngineImpl::Navigate(int dir)
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

void ChmEngineImpl::ZoomTo(float zoomLevel)
{
    if (htmlWindow)
        htmlWindow->SetZoomPercent((int)zoomLevel);
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

        ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(url));
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

bool ChmEngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);
    Timer t(true);
    doc = ChmDoc::CreateFromFile(fileName);
    dbglog::LogF("ChmDoc::CreateFromFile(): %.2f ms", t.GetTimeInMs());
    if (!doc)
        return false;

    // always make the document's homepage page 1
    pages.Append(str::conv::FromAnsi(doc->GetHomePath()));
    // parse the ToC here, since page numbering depends on it
    t.Start();
    ChmTocBuilder tmpTocBuilder(doc, &pages, &tocTrace, &poolAlloc);
    doc->ParseToc(&tmpTocBuilder);
    dbglog::LogF("doc->ParseToc(): %.2f ms", t.GetTimeInMs());
    CrashIf(pages.Count() == 0);
    return pages.Count() > 0;
}

ChmCacheEntry *ChmEngineImpl::FindDataForUrl(const WCHAR *url)
{
    for (size_t i = 0; i < urlDataCache.Count(); i++) {
        ChmCacheEntry *e = urlDataCache.At(i);
        if (str::Eq(url, e->url))
            return e;
    }
    return NULL;
}

// Load and cache data for a given url inside CHM file.
const unsigned char *ChmEngineImpl::GetDataForUrl(const WCHAR *url, size_t *len)
{
    ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(url));
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

void ChmEngineImpl::DownloadData(const WCHAR *url, const unsigned char *data, size_t len)
{
    if (navCb)
        navCb->SaveDownload(url, data, len);
}

PageDestination *ChmEngineImpl::GetNamedDest(const WCHAR *name)
{
    ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(name));
    int pageNo = pages.Find(plainUrl) + 1;
    if (pageNo > 0)
        return new ChmNamedDest(name, pageNo);
    return NULL;
}

// Callers delete the ToC tree, so we re-create it from prerecorded
// values (which is faster than re-creating it from html every time)
DocTocItem *ChmEngineImpl::GetTocTree()
{
    DocTocItem *root = NULL, **nextChild = &root;
    Vec<DocTocItem *> levels;
    int idCounter = 0;

    for (ChmTocTraceItem *ti = tocTrace.IterStart(); ti; ti = tocTrace.IterNext()) {
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

bool ChmEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return ChmDoc::IsSupportedFile(fileName, sniff);
}

ChmEngine *ChmEngine::CreateFromFile(const WCHAR *fileName)
{
    ChmEngineImpl *engine = new ChmEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
