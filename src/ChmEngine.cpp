/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmEngine.h"
#include "ChmDoc.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Vec.h"
#include "HtmlWindow.h"
#include "WinUtil.h"

// when set, always returns false from ChmEngine::IsSupportedFile
// so that an alternative implementation can be used
static bool gDebugAlternateChmEngine = false;

void DebugAlternateChmEngine(bool enable)
{
    gDebugAlternateChmEngine = enable;
}

static bool IsExternalUrl(const TCHAR *url)
{
    return str::StartsWithI(url, _T("http://")) ||
           str::StartsWithI(url, _T("https://")) ||
           str::StartsWithI(url, _T("mailto:"));
}

static TCHAR *ToPlainUrl(const TCHAR *url)
{
    TCHAR *plainUrl = str::Dup(url);
    str::TransChars(plainUrl, _T("#?"), _T("\0\0"));
    return plainUrl;
}

class ChmTocItem : public DocTocItem, public PageDestination {
public:
    ScopedMem<TCHAR> url;

    ChmTocItem(TCHAR *title, int pageNo, TCHAR *url) :
        DocTocItem(title, pageNo), url(url) { }
    ChmTocItem *Clone();

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
    virtual TCHAR *GetDestValue() const {
        if (url && IsExternalUrl(url))
            return str::Dup(url);
        return NULL;
    }
};

ChmTocItem *ChmTocItem::Clone()
{
    ChmTocItem *res = new ChmTocItem(str::Dup(title), pageNo, url ? str::Dup(url) : NULL);
    res->open = open;
    res->id = id;
    if (child)
        res->child = static_cast<ChmTocItem *>(child)->Clone();
    if (next)
        res->next = static_cast<ChmTocItem *>(next)->Clone();
    return res;
}

class ChmCacheEntry {
public:
    TCHAR *url;
    char *data;
    size_t size;

    ChmCacheEntry(const TCHAR *url) : data(NULL), size(0) {
        this->url = str::Dup(url);
    }
    ~ChmCacheEntry() {
        free(url);
        free(data);
    }
};

class ChmEngineImpl : public ChmEngine, public HtmlWindowCallback {
    friend ChmEngine;

public:
    ChmEngineImpl();
    virtual ~ChmEngineImpl();
    virtual ChmEngine *Clone() {
        return CreateFromFile(fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages.Count(); }

    virtual RectD PageMediabox(int pageNo) { return RectD(); }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        return RectD();
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View) {
         // TOOD: assert(0);
         return NULL;
    }

    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
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

    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) {
        return NULL;
    }

    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }
    virtual TCHAR *GetProperty(char *name) { return doc->GetProperty(name); }

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".chm"); }

    virtual bool BenchLoadPage(int pageNo) { return true; }

    virtual PageDestination *GetNamedDest(const TCHAR *name);
    virtual bool HasTocTree() const { return tocRoot != NULL; }
    // Callers delete the ToC tree, so we return a copy
    // (probably faster than re-creating it from html every time)
    virtual DocTocItem *GetTocTree() { return tocRoot ? tocRoot->Clone() : NULL; }

    virtual void SetParentHwnd(HWND hwnd);
    virtual void DisplayPage(int pageNo) { DisplayPage(pages.At(pageNo - 1)); }
    virtual void SetNavigationCalback(ChmNavigationCallback *cb) { navCb = cb; }
    virtual RenderedBitmap *CreateThumbnail(SizeI size);
    virtual void GoToDestination(PageDestination *link);

    virtual void PrintCurrentPage() { if (htmlWindow) htmlWindow->PrintCurrentPage(); }
    virtual void FindInCurrentPage() { if (htmlWindow) htmlWindow->FindInCurrentPage(); }
    virtual bool CanNavigate(int dir);
    virtual void Navigate(int dir);
    virtual void ZoomTo(float zoomLevel);
    virtual void SelectAll() { if (htmlWindow) htmlWindow->SelectAll(); }
    virtual void CopySelection() { if (htmlWindow) htmlWindow->CopySelection(); }
    virtual int CurrentPageNo() const { return currentPageNo; }
    virtual void PassUIMsg(UINT msg, WPARAM wParam, LPARAM lParam) {
        if (htmlWindow) htmlWindow->SendMsg(msg, wParam, lParam);
    }

    // from HtmlWindowCallback
    virtual bool OnBeforeNavigate(const TCHAR *url, bool newWindow);
    virtual void OnDocumentComplete(const TCHAR *url);
    virtual void OnLButtonDown() { if (navCb) navCb->FocusFrame(true); }
    virtual bool GetDataForUrl(const TCHAR *url, char **data, size_t *len);

protected:
    TCHAR *fileName;
    ChmDoc *doc;
    ChmTocItem *tocRoot;

    StrVec pages;
    int currentPageNo;
    HtmlWindow *htmlWindow;
    ChmNavigationCallback *navCb;

    Vec<ChmCacheEntry*> urlDataCache;

    bool Load(const TCHAR *fileName);
    void DisplayPage(const TCHAR *pageUrl);

    ChmCacheEntry *FindDataForUrl(const TCHAR *url);
};

ChmEngineImpl::ChmEngineImpl() :
    fileName(NULL), doc(NULL), tocRoot(NULL),
    htmlWindow(NULL), navCb(NULL), currentPageNo(1)
{
}

ChmEngineImpl::~ChmEngineImpl()
{
    // TODO: deleting htmlWindow seems to spin a modal loop which
    //       can lead to WM_PAINT being dispatched for the parent
    //       hwnd and then crashing in SumatraPDF.cpp's DrawDocument
    delete htmlWindow;
    delete tocRoot;
    delete doc;
    free(fileName);
    DeleteVecMembers(urlDataCache);
}

// Called after html document has been loaded.
// Sync the state of the ui with the page (show
// the right page number, select the right item in toc tree)
void ChmEngineImpl::OnDocumentComplete(const TCHAR *url)
{
    if (*url == _T('/'))
        ++url;
    int pageNo = pages.Find(ScopedMem<TCHAR>(ToPlainUrl(url))) + 1;
    if (pageNo) {
        currentPageNo = pageNo;
        if (navCb)
            navCb->PageNoChanged(pageNo);
    }
}

// Called before we start loading html for a given url. Will block
// loading if returns false.
bool ChmEngineImpl::OnBeforeNavigate(const TCHAR *url, bool newWindow)
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

void ChmEngineImpl::SetParentHwnd(HWND hwnd)
{
    assert(!htmlWindow);
    delete htmlWindow;
    htmlWindow = new HtmlWindow(hwnd, this);
}

void ChmEngineImpl::DisplayPage(const TCHAR *pageUrl)
{
    if (IsExternalUrl(pageUrl)) {
        // open external links in an external browser
        // (same as for PDF, XPS, etc. documents)
        if (navCb)
            navCb->LaunchBrowser(pageUrl);
        return;
    }

    int pageNo = pages.Find(ScopedMem<TCHAR>(ToPlainUrl(pageUrl))) + 1;
    if (pageNo)
        currentPageNo = pageNo;

    // This is a hack that seems to be needed for some chm files where
    // url starts with "..\" even though it's not accepted by ie as
    // a correct its: url. There's a possibility it breaks some other
    // chm files (I don't know such cases, though).
    // A more robust solution would try to match with the actual
    // names of files inside chm package.
    if (str::StartsWith(pageUrl, _T("..\\")))
        pageUrl += 3;

    if (str::StartsWith(pageUrl, _T("/")))
        pageUrl++;

    assert(htmlWindow);
    if (htmlWindow)
        htmlWindow->NavigateToDataUrl(pageUrl);
}

RenderedBitmap *ChmEngineImpl::CreateThumbnail(SizeI size)
{
    RenderedBitmap *bmp = NULL;
    // We render twice the size of thumbnail and scale it down
    RectI area(0, 0, size.dx * 2, size.dy * 2);

    // reusing WC_STATIC. I don't think exact class matters (WndProc
    // will be taken over by HtmlWindow anyway) but it can't be NULL.
    int winDx = area.dx + GetSystemMetrics(SM_CXVSCROLL);
    int winDy = area.dy + GetSystemMetrics(SM_CYHSCROLL);
    HWND hwnd = CreateWindow(WC_STATIC, _T("BrowserCapture"), WS_POPUP,
                             0, 0, winDx, winDy, NULL, NULL, NULL, NULL);
    if (!hwnd)
        return NULL;

#if 0 // when debugging set to 1 to see the window
    ShowWindow(hwnd, SW_SHOW);
#endif
    SetParentHwnd(hwnd);
    DisplayPage(1);
    if (!htmlWindow || !htmlWindow->WaitUntilLoaded(5 * 1000))
        goto Exit;
    HBITMAP hbmp = htmlWindow->TakeScreenshot(area, size);
    if (!hbmp)
        goto Exit;
    bmp = new RenderedBitmap(hbmp, size);

Exit:
    DestroyWindow(hwnd);
    return bmp;
}

void ChmEngineImpl::GoToDestination(PageDestination *link)
{
    ChmTocItem *item = static_cast<ChmTocItem *>(link);
    if (item && item->url)
        DisplayPage(item->url);
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
    StrVec *pages;
    ChmTocItem **root;
    int idCounter;
    Vec<DocTocItem *> lastItems;

    // We fake page numbers by doing a depth-first traversal of
    // toc tree and considering each unique html page in toc tree
    // as a page
    int CreatePageNoForURL(const TCHAR *url) {
        if (!url || IsExternalUrl(url))
            return 0;

        ScopedMem<TCHAR> plainUrl(ToPlainUrl(url));
        int pageNo = pages->Find(plainUrl) + 1;
        if (pageNo > 0)
            return pageNo;

        pages->Append(plainUrl.StealData());
        return pages->Count();
    }

public:
    ChmTocBuilder(ChmDoc *doc, StrVec *pages, ChmTocItem **root) :
        doc(doc), pages(pages), root(root), idCounter(0) { }

    virtual void visit(const TCHAR *name, const TCHAR *url, int level) {
        int pageNo = CreatePageNoForURL(url);
        ChmTocItem *item = new ChmTocItem(str::Dup(name), pageNo, url ? str::Dup(url) : NULL);
        item->id = ++idCounter;
        item->open = level == 1;

        // append the item at the correct level
        CrashIf(level < 1);
        if (!*root) {
            *root = item;
            lastItems.Append(*root);
        } else if ((size_t)level <= lastItems.Count()) {
            lastItems.RemoveAt(level, lastItems.Count() - level);
            lastItems.Last() = lastItems.Last()->next = item;
        } else {
            lastItems.Last()->child = item;
            lastItems.Append(item);
        }
    }
};

bool ChmEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);
    doc = ChmDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    // always make the document's homepage page 1
    pages.Append(str::conv::FromAnsi(doc->GetIndexPath()));
    // parse the ToC here, since page numbering depends on it
    doc->ParseToc(&ChmTocBuilder(doc, &pages, &tocRoot));

    CrashIf(pages.Count() == 0);
    return pages.Count() > 0;
}

ChmCacheEntry *ChmEngineImpl::FindDataForUrl(const TCHAR *url)
{
    for (size_t i = 0; i < urlDataCache.Count(); i++) {
        ChmCacheEntry *e = urlDataCache.At(i);
        if (str::Eq(url, e->url))
            return e;
    }
    return NULL;
}

// Load and cache data for a given url inside CHM file.
bool ChmEngineImpl::GetDataForUrl(const TCHAR *url, char **data, size_t *len)
{
    ScopedMem<TCHAR> plainUrl(ToPlainUrl(url));
    ChmCacheEntry *e = FindDataForUrl(plainUrl);
    if (!e) {
        e = new ChmCacheEntry(plainUrl);
        ScopedMem<char> urlAnsi(str::conv::ToAnsi(plainUrl));
        e->data = (char *)doc->GetData(urlAnsi, &e->size);
        if (!e->data) {
            delete e;
            return false;
        }
        urlDataCache.Append(e);
    }
    *data = e->data;
    *len = e->size;
    return true;
}

PageDestination *ChmEngineImpl::GetNamedDest(const TCHAR *name)
{
    ScopedMem<TCHAR> plainUrl(ToPlainUrl(name));
    int pageNo = pages.Find(plainUrl) + 1;
    if (pageNo > 0)
        return new ChmTocItem(NULL, pageNo, str::Dup(name));
    return NULL;
}

bool ChmEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (gDebugAlternateChmEngine)
        return false;
    return ChmDoc::IsSupportedFile(fileName, sniff);
}

ChmEngine *ChmEngine::CreateFromFile(const TCHAR *fileName)
{
    ChmEngineImpl *engine = new ChmEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
