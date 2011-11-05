/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Vec.h"
#include "TrivialHtmlParser.h"
#include "HtmlWindow.h"

#define CHM_MT
#define PPC_BSTR

#include <inttypes.h>
#include <chm_lib.h>

class ChmTocItem : public DocTocItem, public PageDestination {
public:
    ChmTocItem(TCHAR *title) : DocTocItem(title) { }

    virtual PageDestination *GetLink() { return this; }
    virtual const char *GetType() const { return "ScrollTo"; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return RectD(); }
    virtual TCHAR *GetDestValue() const { return NULL; }

    static ChmTocItem *Clone(DocTocItem *item);
};

ChmTocItem *ChmTocItem::Clone(DocTocItem *item)
{
    if (!item)
        return NULL;

    ChmTocItem *res = new ChmTocItem(str::Dup(item->title));
    res->open = item->open;
    res->pageNo = item->pageNo;
    res->id = item->id;
    res->child = Clone(item->child);
    res->next = Clone(item->next);
    return res;
}

// Data parsed from /#WINDOWS, /#STRINGS, /#SYSTEM files inside CHM file
class ChmInfo {
public:
    ChmInfo() : title(NULL), tocPath(NULL), indexPath(NULL), homePath(NULL)
    {}
    ~ChmInfo() {
        free(title);
        free(tocPath);
        free(indexPath);
        free(homePath);
    }
    char *title;
    char *tocPath;
    char *indexPath;
    char *homePath;
};

class CChmEngine : public ChmEngine, public HtmlWindowCallback {
    friend ChmEngine;

public:
    CChmEngine();
    virtual ~CChmEngine();
    virtual ChmEngine *Clone() {
        return CreateFromFileName(fileName);
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

    virtual bool IsImagePage(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".chm"); }

    virtual bool BenchLoadPage(int pageNo) { return true; }

    virtual bool HasTocTree() const { return tocRoot != NULL; }
    // Callers delete the ToC tree, so we return a copy
    // (probably faster than re-creating it from html every time)
    virtual DocTocItem *GetTocTree() { return ChmTocItem::Clone(tocRoot); }

    virtual void SetParentHwnd(HWND hwnd);
    virtual void DisplayPage(int pageNo) { DisplayPage(pages.At(pageNo - 1)); }
    virtual void SetNavigationCalback(ChmNavigationCallback *cb) { navCb = cb; }
    virtual RenderedBitmap *CreateThumbnail(SizeI size);
    virtual void PrintCurrentPage() { htmlWindow->PrintCurrentPage(); }

    // from HtmlWindowCallback
    virtual bool OnBeforeNavigate(const TCHAR *url);

protected:
    const TCHAR *fileName;
    struct chmFile *chmHandle;
    ChmInfo *chmInfo;
    ChmTocItem *tocRoot;

    StrVec pages;
    HtmlWindow *htmlWindow;
    ChmNavigationCallback *navCb;

    bool Load(const TCHAR *fileName);
    void DisplayPage(const TCHAR *pageUrl);
};

CChmEngine::CChmEngine() :
    fileName(NULL), chmHandle(NULL), chmInfo(NULL), tocRoot(NULL),
        htmlWindow(NULL), navCb(NULL)
{
}

// called when we're about to show a given url. If this is a CHM
// html page, sync the state of the ui with the page (show
// the right page number, select the right item in toc tree)
bool CChmEngine::OnBeforeNavigate(const TCHAR *url)
{
    // url format for chm page is: "its:MyChmFile.chm::mywebpage.htm"
    // we need to extract the "mywebpage.htm" part
    if (!str::StartsWithI(url, _T("its:")))
        return true;
    url += 4;
    // you might be tempted to just test if url starts with fileName,
    // but it looks like browser control can url-escape fileName, so
    // we'll just look for "::"
    url = str::Find(url, _T("::"));
    if (!url)
        return true;
    url += 2;
    if (*url == _T('/'))
        ++url;
    
    int pageNo = pages.Find(url) + 1;
    if (pageNo > 0 && navCb)
        navCb->UpdatePageNo(pageNo);
    return true;
}

void CChmEngine::SetParentHwnd(HWND hwnd)
{
    assert(!htmlWindow);
    delete htmlWindow;
    htmlWindow = new HtmlWindow(hwnd, this);
}

void CChmEngine::DisplayPage(const TCHAR *pageUrl)
{
    if (str::StartsWith(pageUrl, _T("/")))
        pageUrl++;

    // the format for chm page is: "its:MyChmFile.chm::mywebpage.htm"
    // cf. http://msdn.microsoft.com/en-us/library/aa164814(v=office.10).aspx
    ScopedMem<TCHAR> url(str::Format(_T("its:%s::/%s"), fileName, pageUrl));
    htmlWindow->NavigateToUrl(url);
    //htmlWindow->DisplayHtml(_T("<html><body>Hello!</body></html>"));
}

RenderedBitmap *CChmEngine::CreateThumbnail(SizeI size)
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
    if (!htmlWindow->WaitUntilLoaded(5 * 1000))
        goto Exit;
    HBITMAP hbmp = htmlWindow->TakeScreenshot(area, size);
    if (!hbmp)
        goto Exit;
    bmp = new RenderedBitmap(hbmp, size.dx, size.dy);

Exit:
    DestroyWindow(hwnd);
    return bmp;
}

CChmEngine::~CChmEngine()
{
    chm_close(chmHandle);
    delete chmInfo;
    // TODO: deleting htmlWindow seems to spin a modal loop which
    //       can lead to WM_PAINT being dispatched for the parent
    //       hwnd and then crashing in SumatraPDF.cpp's DrawDocument
    delete htmlWindow;
    delete tocRoot;
    free((void *)fileName);
}

CASSERT(1 == sizeof(uint8_t), uint8_is_1_byte);
CASSERT(2 == sizeof(uint16_t), uint16_is_2_bytes);
CASSERT(4 == sizeof(uint32_t), uint32_is_4_bytes);

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
    char *strEnd = (char *)memchr(strStart, 0, b.size);
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
            {
                char *compiler = ReadString(b, off);
                free(compiler);
            }
            break;
        case 16:
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
static int CreatePageNoForURL(StrVec& pages, ScopedMem<TCHAR>& url)
{
    if (!url)
        return 0;

    int pageNo = pages.Find(url);
    if (pageNo != -1)
        return pageNo + 1;

    pages.Append(url.StealData());
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
ChmTocItem *TocItemFromLi(StrVec& pages, HtmlElement *el)
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
        else if (str::EqI(attrName, _T("Name")))
            name.Set(attrVal.StealData());
        else if (str::EqI(attrName, _T("Local")))
            local.Set(attrVal.StealData());
    }
    if (!name)
        return NULL;
    // remove the ITS protocol and any filename references from the URLs
    if (local && str::Find(local, _T("::/")))
        local.Set(str::Dup(str::Find(local, _T("::/")) + 3));

    ChmTocItem *item = new ChmTocItem(name.StealData());
    item->pageNo = CreatePageNoForURL(pages, local);
    return item;
}

ChmTocItem *BuildChmToc(StrVec& pages, HtmlElement *list)
{
    assert(str::Eq("ul", list->name));
    ChmTocItem *node = NULL;

    for (HtmlElement *el = list->down; el; el = el->next) {
        if (!str::Eq(el->name, "li"))
            continue; // ignore unexpected elements
        ChmTocItem *item = TocItemFromLi(pages, el);
        if (!item)
            continue; // skip incomplete elements and all their children

        HtmlElement *nested = el->GetChildByName("ul");
        if (nested)
            item->child = BuildChmToc(pages, nested);

        if (!node)
            node = item;
        else
            node->AddSibling(item);
    }

    return node;
}

static ChmTocItem *ParseChmHtmlToc(StrVec& pages, char *html)
{
    HtmlParser p;
    HtmlElement *el = p.Parse(html);
    if (!el)
        return NULL;
    el = p.FindElementByName("body");
    if (!el)
        return NULL;
    el = p.FindElementByName("ul", el);
    if (!el)
        return NULL;
    return BuildChmToc(pages, el);
}

bool CChmEngine::Load(const TCHAR *fileName)
{
    assert(NULL == chmHandle);
    this->fileName = str::Dup(fileName);
    CASSERT(2 == sizeof(OLECHAR), OLECHAR_must_be_WCHAR);
#ifdef UNICODE
    chmHandle = chm_open((WCHAR *)fileName);
#else
    chmHandle = chm_open(ScopedMem<WCHAR>(str::conv::FromAnsi(fileName)));
#endif
    if (!chmHandle)
        return false;
    chmInfo = new ChmInfo();
    ParseWindowsChmData(chmHandle, chmInfo);
    if (!ParseSystemChmData(chmHandle, chmInfo))
        return false;

    if (!chmInfo->homePath || !ChmFileExists(chmHandle, chmInfo->homePath))
        chmInfo->homePath = FindHomeForPath(chmHandle, "/");
    if (!chmInfo->homePath)
        return false;

    // always make the document's homepage page 1
    pages.Append(str::conv::FromAnsi(chmInfo->homePath));

    if (chmInfo->tocPath) {
        // parse the ToC here, since page numbering depends on it
        Bytes b;
        if (GetChmDataForFile(chmHandle, chmInfo->tocPath, b))
            tocRoot = ParseChmHtmlToc(pages, (char *)b.d);
    }

    return true;
}

unsigned char *CChmEngine::GetFileData(size_t *cbCount)
{
    return (unsigned char *)file::ReadAll(fileName, cbCount);
}

bool ChmEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (sniff)
        return file::StartsWith(fileName, "ITSF");

    return str::EndsWithI(fileName, _T(".chm"));
}

ChmEngine *ChmEngine::CreateFromFileName(const TCHAR *fileName)
{
    CChmEngine *engine = new CChmEngine();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
