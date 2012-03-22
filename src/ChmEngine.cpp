/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmEngine.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Vec.h"
#include "TrivialHtmlParser.h"
#include "HtmlWindow.h"
#include "WinUtil.h"
// for GetChmCodepage
#include "ChmDoc.h"

#define CHM_MT
#ifdef UNICODE
#define PPC_BSTR
#endif

#include <inttypes.h>
#include <chm_lib.h>

// when set, always returns false from ChmEngine::IsSupportedFile
// so that an alternative implementation can be used
static bool gDebugAlternateChmEngine = false;

void DebugAlternateChmEngine(bool enable)
{
    gDebugAlternateChmEngine = enable;
}

STATIC_ASSERT(1 == sizeof(uint8_t), uint8_is_1_byte);
STATIC_ASSERT(2 == sizeof(uint16_t), uint16_is_2_bytes);
STATIC_ASSERT(4 == sizeof(uint32_t), uint32_is_4_bytes);

class Bytes {
public:
    Bytes() : d(NULL), size(0)
    {}
    ~Bytes() {
        free(d);
    }
    uint8_t *d;
    size_t size;
};

static bool IsExternalUrl(const TCHAR *url)
{
    return str::StartsWithI(url, _T("http://")) ||
           str::StartsWithI(url, _T("https://")) ||
           str::StartsWithI(url, _T("mailto:"));
}

class ChmTocItem : public DocTocItem, public PageDestination {
public:
    ScopedMem<TCHAR> url;

    ChmTocItem(TCHAR *title, int pageNo, TCHAR *url) :
        DocTocItem(title, pageNo), url(url) { }
    ChmTocItem *Clone();

    virtual PageDestination *GetLink() { return this; }
    virtual const char *GetDestType() const {
        if (url && IsExternalUrl(url))
            return "LaunchURL";
        return "ScrollTo";
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

// Data parsed from /#WINDOWS, /#STRINGS, /#SYSTEM files inside CHM file
class ChmInfo {
public:
    ChmInfo() : title(NULL), tocPath(NULL), indexPath(NULL),
        homePath(NULL), creator(NULL), codepage(CP_ACP) { }
    ~ChmInfo() {
        free(title);
        free(tocPath);
        free(indexPath);
        free(homePath);
        free(creator);
    }
    char *title;
    char *tocPath;
    char *indexPath;
    char *homePath;
    char *creator;
    UINT codepage;
};

class ChmCacheEntry {
public:
    TCHAR *url;
    Bytes data;

    ChmCacheEntry(const TCHAR *url) {
        this->url = str::Dup(url);
    }
    ~ChmCacheEntry() {
        free(url);
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
    virtual unsigned char *GetFileData(size_t *cbCount);

    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) {
        return NULL;
    }

    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }
    virtual TCHAR *GetProperty(char *name);

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

    Bytes *FindDataForUrl(const TCHAR *url);

protected:
    const TCHAR *fileName;
    struct chmFile *chmHandle;
    ChmInfo chmInfo;
    ChmTocItem *tocRoot;

    StrVec pages;
    int currentPageNo;
    HtmlWindow *htmlWindow;
    ChmNavigationCallback *navCb;

    Vec<ChmCacheEntry*> urlDataCache;

    bool Load(const TCHAR *fileName);
    void DisplayPage(const TCHAR *pageUrl);
};

ChmEngineImpl::ChmEngineImpl() :
    fileName(NULL), chmHandle(NULL), tocRoot(NULL),
    htmlWindow(NULL), navCb(NULL), currentPageNo(1)
{
}

Bytes *ChmEngineImpl::FindDataForUrl(const TCHAR *url)
{
    for (size_t i = 0; i < urlDataCache.Count(); i++) {
        ChmCacheEntry *e = urlDataCache.At(i);
        if (str::Eq(url, e->url))
            return &e->data;
    }

    return NULL;
}

static TCHAR *ToPlainUrl(const TCHAR *url)
{
    TCHAR *plainUrl = str::Dup(url);
    str::TransChars(plainUrl, _T("#?"), _T("\0\0"));
    return plainUrl;
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

ChmEngineImpl::~ChmEngineImpl()
{
    chm_close(chmHandle);
    // TODO: deleting htmlWindow seems to spin a modal loop which
    //       can lead to WM_PAINT being dispatched for the parent
    //       hwnd and then crashing in SumatraPDF.cpp's DrawDocument
    delete htmlWindow;
    delete tocRoot;
    free((void *)fileName);
    DeleteVecMembers(urlDataCache);
}

// The numbers in CHM format are little-endian
static bool ReadU16(const Bytes& b, size_t off, uint16_t& valOut)
{
    if (off + sizeof(uint16_t) > b.size)
        return false;
    valOut = b.d[off] | (b.d[off+1] << 8);
    return true;
}

// The numbers in CHM format are little-endian
static bool ReadU32(const Bytes& b, size_t off, uint32_t& valOut)
{
    if (off + sizeof(uint32_t) > b.size)
        return false;
    valOut = b.d[off] | (b.d[off+1] << 8) | (b.d[off+2] << 16) | (b.d[off+3] << 24);
    return true;
}

static char *ReadString(const Bytes& b, size_t off)
{
    if (off >= b.size)
        return NULL;
    char *strStart = (char *)b.d + off;
    char *strEnd = (char *)memchr(strStart, 0, b.size - off);
    // didn't find terminating 0 - assume it's corrupted
    if (!strEnd || !*strStart)
        return NULL;
    return str::Dup(strStart);
}

static char *ReadWsTrimmedString(const Bytes& b, size_t off)
{
    char *s = ReadString(b, off);
    if (s) {
        str::RemoveChars(s, "\n\r\t");
    }
    return s;
}

static bool GetChmDataForFile(struct chmFile *chmHandle, const char *fileName, Bytes& dataOut)
{
    ScopedMem<const char> fileNameTmp;
    if (!str::StartsWith(fileName, "/")) {
        fileNameTmp.Set(str::Join("/", fileName));
        fileName = fileNameTmp;
    } else if (str::StartsWith(fileName, "///")) {
        fileName += 2;
    }

    struct chmUnitInfo info;
    int res = chm_resolve_object(chmHandle, fileName, &info);
    if (CHM_RESOLVE_SUCCESS != res) {
        return false;
    }

    dataOut.size = (size_t)info.length;
    if (dataOut.size > 128*1024*1024) {
        // don't allow anything above 128 MB
        return false;
    }

    // +1 for 0 terminator for C string compatibility
    dataOut.d = (uint8_t *)malloc(dataOut.size + 1);
    if (!dataOut.d)
        return false;

    if (!chm_retrieve_object(chmHandle, &info, dataOut.d, 0, dataOut.size)) {
        return false;
    }
    dataOut.d[dataOut.size] = 0;
    return true;
}

static bool ChmFileExists(struct chmFile *chmHandle, const char *path)
{
    ScopedMem<char> path2;
    if (!str::StartsWith(path, "/"))
        path2.Set(str::Join("/", path));
    else if (str::StartsWith(path, "///"))
        path += 2;

    struct chmUnitInfo info;
    return chm_resolve_object(chmHandle, path2 ? path2 : path, &info) == CHM_RESOLVE_SUCCESS;
}

static char *FindHomeForPath(struct chmFile *chmHandle, const char *basePath)
{
    const char *pathsToTest[] = {
        "index.htm", "index.html",
        "default.htm", "default.html"
    };

    const char *sep = str::EndsWith(basePath, "/") ? "" : "/";
    for (int i = 0; i < dimof(pathsToTest); i++) {
        ScopedMem<char> testPath(str::Format("%s%s%s", basePath, sep, pathsToTest[i]));
        if (ChmFileExists(chmHandle, testPath))
            return testPath.StealData();
    }
    return NULL;
}

// http://www.nongnu.org/chmspec/latest/Internal.html#WINDOWS
static void ParseWindowsChmData(chmFile *chmHandle, ChmInfo *chmInfo)
{
    Bytes windowsBytes;
    Bytes stringsBytes;
    bool hasWindows = GetChmDataForFile(chmHandle, "/#WINDOWS", windowsBytes);
    bool hasStrings = GetChmDataForFile(chmHandle, "/#STRINGS", stringsBytes);
    if (!hasWindows || !hasStrings)
        return;

    uint32_t entries, entrySize, strOff;
    bool ok = ReadU32(windowsBytes, 0, entries);
    if (!ok)
        return;
    ok = ReadU32(windowsBytes, 4, entrySize);
    if (!ok)
        return;

    for (uint32_t i = 0; i < entries; ++i) {
        uint32_t off = 8 + (i * entrySize);
        if (!chmInfo->title) {
            ok = ReadU32(windowsBytes, off + 0x14, strOff);
            if (ok) {
                chmInfo->title = ReadWsTrimmedString(stringsBytes, strOff);
            }
        }
        if (!chmInfo->tocPath) {
            ok = ReadU32(windowsBytes, off + 0x60, strOff);
            if (ok) {
                chmInfo->tocPath = ReadString(stringsBytes, strOff);
            }
        }
        if (!chmInfo->indexPath) {
            ok = ReadU32(windowsBytes, off + 0x64, strOff);
            if (ok) {
                chmInfo->indexPath = ReadString(stringsBytes, strOff);
            }
        }
        if (!chmInfo->homePath) {
            ok = ReadU32(windowsBytes, off+0x68, strOff);
            if (ok) {
                chmInfo->homePath = ReadString(stringsBytes, strOff);
            }
        }
    }
}

// http://www.nongnu.org/chmspec/latest/Internal.html#SYSTEM
static bool ParseSystemChmData(chmFile *chmHandle, ChmInfo *chmInfo)
{
    Bytes b;
    uint16_t type, len;
    bool ok = GetChmDataForFile(chmHandle, "/#SYSTEM", b);
    if (!ok)
        return false;
    uint16_t off = 4;
    // Note: skipping uint32_t version at offset 0. It's supposed to be 2 or 3.
    while (off < b.size) {
        // Note: at some point we seem to get off-sync i.e. I'm seeing many entries
        // with type==0 and len==0. Seems harmless.
        ok = ReadU16(b, off, type);
        if (!ok)
            return true;
        ok = ReadU16(b, off+2, len);
        if (!ok)
            return true;
        off += 4;
        switch (type) {
        case 0:
            if (!chmInfo->tocPath && len > 0) {
                chmInfo->tocPath = ReadString(b, off);
            }
            break;
        case 1:
            if (!chmInfo->indexPath) {
                chmInfo->indexPath = ReadString(b, off);
            }
            break;
        case 2:
            if (!chmInfo->homePath) {
                chmInfo->homePath = ReadString(b, off);
            }
            break;
        case 3:
            if (!chmInfo->title) {
                chmInfo->title = ReadWsTrimmedString(b, off);
            }
            break;

        case 6:
            // for now for debugging
            {
                char *compiledFile = ReadString(b, off);
                free(compiledFile);
            }
            break;
        case 9:
            if (!chmInfo->creator) {
                chmInfo->creator = ReadWsTrimmedString(b, off);
            }
            break;
        case 16:
            // for now for debugging
            {
                char *defaultFont = ReadString(b, off);
                free(defaultFont);
            }
            break;
        }
        off += len;
    }
    return true;
}

// We fake page numbers by doing a depth-first traversal of
// toc tree and considering each unique html page in toc tree
// as a page
static int CreatePageNoForURL(StrVec& pages, const TCHAR *url)
{
    if (!url || IsExternalUrl(url))
        return 0;

    ScopedMem<TCHAR> plainUrl(ToPlainUrl(url));
    int pageNo = pages.Find(plainUrl) + 1;
    if (pageNo > 0)
        return pageNo;

    pages.Append(plainUrl.StealData());
    return pages.Count();
}

/* The html looks like:
<li>
  <object type="text/sitemap">
    <param name="Name" value="Main Page">
    <param name="Local" value="0789729717_main.html">
    <param name="ImageNumber" value="12">
  </object>
  <ul> ... children ... </ul>
<li>
  ... siblings ...
*/
static ChmTocItem *TocItemFromLi(StrVec& pages, HtmlElement *el, UINT cp)
{
    assert(str::Eq("li", el->name));
    el = el->GetChildByName("object");
    if (!el)
        return NULL;

    ScopedMem<TCHAR> name, local;
    for (el = el->GetChildByName("param"); el; el = el->next) {
        if (!str::Eq("param", el->name))
            continue;
        ScopedMem<TCHAR> attrName(el->GetAttribute("name"));
        ScopedMem<TCHAR> attrVal(el->GetAttribute("value"));
        if (!attrName || !attrVal)
            /* ignore incomplete/unneeded <param> */;
        else if (str::EqI(attrName, _T("Name"))) {
#ifdef UNICODE
            if (cp != CP_CHM_DEFAULT) {
                ScopedMem<char> bytes(str::conv::ToCodePage(attrVal, CP_CHM_DEFAULT));
                attrVal.Set(str::conv::FromCodePage(bytes, cp));
            }
#endif
            name.Set(attrVal.StealData());
        }
        else if (str::EqI(attrName, _T("Local")))
            local.Set(attrVal.StealData());
    }
    if (!name)
        return NULL;
    // remove the ITS protocol and any filename references from the URLs
    if (local && str::Find(local, _T("::/")))
        local.Set(str::Dup(str::Find(local, _T("::/")) + 3));

    int pageNo = CreatePageNoForURL(pages, local);
    return new ChmTocItem(name.StealData(), pageNo, local.StealData());
}

static ChmTocItem *BuildChmToc(StrVec& pages, HtmlElement *list, UINT cp, int& idCounter, bool topLevel)
{
    assert(str::Eq("ul", list->name));
    ChmTocItem *node = NULL;

    // some broken ToCs wrap every <li> into its own <ul>
    for (; list && str::Eq(list->name, "ul"); list = list->next) {
        for (HtmlElement *el = list->down; el; el = el->next) {
            if (!str::Eq(el->name, "li"))
                continue; // ignore unexpected elements
            ChmTocItem *item = TocItemFromLi(pages, el, cp);
            if (!item)
                continue; // skip incomplete elements and all their children
            item->id = ++idCounter;

            HtmlElement *nested = el->GetChildByName("ul");
            // some broken ToCs have the <ul> follow right *after* a <li>
            if (!nested && el->next && str::Eq(el->next->name, "ul"))
                nested = el->next;
            if (nested)
                item->child = BuildChmToc(pages, nested, cp, idCounter, false);
            item->open = topLevel;

            if (!node)
                node = item;
            else
                node->AddSibling(item);
        }
    }

    return node;
}

static ChmTocItem *ParseChmHtmlToc(StrVec& pages, char *html, UINT cp)
{
    HtmlParser p;
    // detect UTF-8 content by BOM
    if (str::StartsWith(html, "\xEF\xBB\xBF")) {
        html += 3;
        cp = CP_UTF8;
    }
    // enforce the default codepage, so that pre-encoded text and
    // entities are in the same codepage and TocItemFromLi yields
    // consistent results
    HtmlElement *el = p.Parse(html, CP_CHM_DEFAULT);
    if (!el)
        return NULL;
    el = p.FindElementByName("body");
    // since <body> is optional, also continue without one
    el = p.FindElementByName("ul", el);
    if (!el)
        return NULL;
    int idCounter = 0;
    return BuildChmToc(pages, el, cp, idCounter, true);
}

bool ChmEngineImpl::Load(const TCHAR *fileName)
{
    assert(NULL == chmHandle);

    this->fileName = str::Dup(fileName);
    chmHandle = chm_open((TCHAR *)fileName);
    if (!chmHandle)
        return false;
    ParseWindowsChmData(chmHandle, &chmInfo);
    if (!ParseSystemChmData(chmHandle, &chmInfo))
        return false;

    if (!chmInfo.homePath || !ChmFileExists(chmHandle, chmInfo.homePath))
        chmInfo.homePath = FindHomeForPath(chmHandle, "/");
    if (!chmInfo.homePath)
        return false;

    UINT codepage = GetChmCodepage(fileName);
    if (GetACP() != codepage)
        chmInfo.codepage = codepage;

    // always make the document's homepage page 1
    pages.Append(str::conv::FromCodePage(chmInfo.homePath, chmInfo.codepage));

    if (chmInfo.tocPath) {
        // parse the ToC here, since page numbering depends on it
        Bytes b;
        if (GetChmDataForFile(chmHandle, chmInfo.tocPath, b))
            tocRoot = ParseChmHtmlToc(pages, (char *)b.d, codepage);
    }

    return true;
}

// Load and cache data for a given url inside CHM file.
bool ChmEngineImpl::GetDataForUrl(const TCHAR *url, char **data, size_t *len)
{
    ScopedMem<TCHAR> plainUrl(ToPlainUrl(url));
    Bytes *b = FindDataForUrl(plainUrl);
    if (!b) {
        ChmCacheEntry *e = new ChmCacheEntry(plainUrl);
        ScopedMem<char> urlAnsi(str::conv::ToAnsi(plainUrl));
        bool ok = GetChmDataForFile(chmHandle, urlAnsi, e->data);
        if (!ok) {
            delete e;
            return false;
        }
        b = &e->data;
        urlDataCache.Append(e);
    }
    *data = (char*)b->d;
    *len = b->size;
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

TCHAR *ChmEngineImpl::GetProperty(char *name)
{
    if (str::Eq(name, "Title") && chmInfo.title)
        return str::conv::FromCodePage(chmInfo.title, chmInfo.codepage);
    if (str::Eq(name, "Creator") && chmInfo.creator)
        return str::conv::FromCodePage(chmInfo.creator, chmInfo.codepage);
    return NULL;
}

unsigned char *ChmEngineImpl::GetFileData(size_t *cbCount)
{
    return (unsigned char *)file::ReadAll(fileName, cbCount);
}

bool ChmEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (gDebugAlternateChmEngine)
        return false;

    if (sniff)
        return file::StartsWith(fileName, "ITSF");

    return str::EndsWithI(fileName, _T(".chm"));
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
