/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ChmEngine.h"
#include "ChmDoc.h"
#include "DebugLog.h"
#include "Dict.h"
#include "FileUtil.h"
#include "HtmlWindow.h"
#include "Timer.h"

// when set, always returns false from ChmEngine::IsSupportedFile
// so that an alternative implementation can be used
static bool gDebugAlternateChmEngine = false;

void DebugAlternateChmEngine(bool enable)
{
    gDebugAlternateChmEngine = enable;
}

static bool IsExternalUrl(const WCHAR *url)
{
    return str::StartsWithI(url, L"http://") ||
           str::StartsWithI(url, L"https://") ||
           str::StartsWithI(url, L"mailto:");
}

class ChmTocItem : public DocTocItem, public PageDestination {
public:
    ScopedMem<WCHAR> url;

    ChmTocItem(WCHAR *title, int pageNo, WCHAR *url) :
        DocTocItem(title, pageNo), url(url) { }
    ChmTocItem *Clone() const;

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

ChmTocItem *ChmTocItem::Clone() const
{
    ChmTocItem *res = new ChmTocItem(str::Dup(title), pageNo, str::Dup(url));
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
    WCHAR *url;
    char *data;
    size_t size;

    ChmCacheEntry(const WCHAR *url) : data(NULL), size(0) {
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

    virtual const WCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages.Count(); }
    virtual RectD PageMediabox(int pageNo) { return RectD(); }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL) {
         // TOOD: assert(0);
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
    virtual bool HasTocTree() const { return tocRoot != NULL; }
    // Callers delete the ToC tree, so we return a copy
    // (probably faster than re-creating it from html every time)
    virtual DocTocItem *GetTocTree() { return tocRoot ? tocRoot->Clone() : NULL; }

    virtual void SetParentHwnd(HWND hwnd);
    virtual void DisplayPage(int pageNo) { DisplayPage(pages.At(pageNo - 1)); }
    virtual void SetNavigationCalback(ChmNavigationCallback *cb) { navCb = cb; }

    virtual void GoToDestination(PageDestination *link);
    virtual RenderedBitmap *TakeScreenshot(RectI area, SizeI targetSize);

    virtual void PrintCurrentPage() { if (htmlWindow) htmlWindow->PrintCurrentPage(); }
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
    virtual bool GetDataForUrl(const WCHAR *url, char **data, size_t *len);

protected:
    WCHAR *fileName;
    ChmDoc *doc;
    ChmTocItem *tocRoot;

    WStrList pages;
    int currentPageNo;
    HtmlWindow *htmlWindow;
    ChmNavigationCallback *navCb;

    Vec<ChmCacheEntry*> urlDataCache;

    bool Load(const WCHAR *fileName);
    void DisplayPage(const WCHAR *pageUrl);

    ChmCacheEntry *FindDataForUrl(const WCHAR *url);
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

void ChmEngineImpl::SetParentHwnd(HWND hwnd)
{
    CrashIf(htmlWindow);
    delete htmlWindow;
    htmlWindow = new HtmlWindow(hwnd, this);
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

#define USE_STR_INT_MAP

class ChmTocBuilder : public EbookTocVisitor {
    ChmDoc *doc;

    WStrList *pages;
    ChmTocItem **root;
    int idCounter;
    Vec<DocTocItem *> lastItems;

#ifndef USE_STR_INT_MAP
    // We fake page numbers by doing a depth-first traversal of
    // toc tree and considering each unique html page in toc tree
    // as a page
    int CreatePageNoForURL(const WCHAR *url) {
        if (!url || IsExternalUrl(url))
            return 0;

        ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(url));
        int pageNo = pages->Find(plainUrl) + 1;
        if (pageNo > 0)
            return pageNo;
        pages->Append(plainUrl.StealData());
        return pages->Count();
    }
#else
    // TODO: could use dict::MapWStrToInt instead of StrList in the caller
    // as well
    dict::MapWStrToInt urlsSet;

    int CreatePageNoForURL(const WCHAR *url) {
        if (!url || IsExternalUrl(url))
            return 0;

        ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(url));
        int pageNo = pages->Count() + 1;
        bool inserted = urlsSet.Insert(plainUrl, pageNo, &pageNo);
        if (inserted) {
            pages->Append(plainUrl.StealData());
            CrashIf(pageNo != pages->Count());
        } else {
            CrashIf(pageNo == pages->Count() + 1);
        }
        return pageNo;
    }
#endif

public:
    ChmTocBuilder(ChmDoc *doc, WStrList *pages, ChmTocItem **root) :
        doc(doc), pages(pages), root(root), idCounter(0)
        {
#ifdef USE_STR_INT_MAP
            for (size_t i = 0; i < pages->Count(); i++) {
                const WCHAR *url = pages->At(i);
                bool inserted = urlsSet.Insert(url, i + 1, NULL);
                CrashIf(!inserted);
            }
#endif
        }

    virtual void Visit(const WCHAR *name, const WCHAR *url, int level) {
        int pageNo = CreatePageNoForURL(url);
        ChmTocItem *item = new ChmTocItem(str::Dup(name), pageNo, str::Dup(url));
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
    doc->ParseToc(&ChmTocBuilder(doc, &pages, &tocRoot));
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
bool ChmEngineImpl::GetDataForUrl(const WCHAR *url, char **data, size_t *len)
{
    ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(url));
    ChmCacheEntry *e = FindDataForUrl(plainUrl);
    if (!e) {
        e = new ChmCacheEntry(plainUrl);
        ScopedMem<char> urlUtf8(str::conv::ToUtf8(plainUrl));
        e->data = (char *)doc->GetData(urlUtf8, &e->size);
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

PageDestination *ChmEngineImpl::GetNamedDest(const WCHAR *name)
{
    ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(name));
    int pageNo = pages.Find(plainUrl) + 1;
    if (pageNo > 0)
        return new ChmTocItem(NULL, pageNo, str::Dup(name));
    return NULL;
}

bool ChmEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (gDebugAlternateChmEngine)
        return false;
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
