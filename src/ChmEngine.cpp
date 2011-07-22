/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Vec.h"
#include "TrivialHtmlParser.h"

#include "chm_lib.h"

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

class ChmToCItem : public DocToCItem {
public:
    TCHAR *url;
    TCHAR *imageNumber;

    ChmToCItem(TCHAR *title, TCHAR *url, TCHAR *imageNumber) :
        DocToCItem(title), url(url), imageNumber(imageNumber)
    {
    }

    ~ChmToCItem() {
        free(url);
        free(imageNumber);
    }

    virtual PageDestination *GetLink() { return NULL; }
};

class CChmEngine : public ChmEngine {
    friend ChmEngine;

public:
    CChmEngine();
    virtual ~CChmEngine();
    virtual ChmEngine *Clone() {
        return CreateFromFileName(fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages.Count(); }

    virtual RectD PageMediabox(int pageNo) {
        RectD r; return r;
    }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        RectD r; return r;
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

    // we always have toc tree
    virtual bool HasToCTree() const { return true; }
    virtual DocToCItem *GetToCTree() { return tocRoot; }

protected:
    const TCHAR *fileName;
    struct chmFile *chmHandle;
    ChmInfo *chmInfo;
    ChmToCItem *tocRoot;

    Vec<ChmToCItem*> pages; // point to one of the items within tocRoot

    bool Load(const TCHAR *fileName);
    bool LoadAndParseHtmlToc();
    bool ParseChmHtmlToc(char *html);
};

CChmEngine::CChmEngine() :
    fileName(NULL), chmHandle(NULL), chmInfo(NULL), tocRoot(NULL)
{
}

CChmEngine::~CChmEngine()
{
    chm_close(chmHandle);
    delete chmInfo;
    free((void *)fileName);
}

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

CASSERT(2 == sizeof(u16), u16_is_2_bytes);
CASSERT(4 == sizeof(u32), u32_is_4_bytes);

class Bytes {
public:
    Bytes() : d(NULL), size(0)
    {}
    ~Bytes() {
        free(d);
    }
    u8 *d;
    size_t size;
};

// The numbers in CHM format are little-endian
static bool ReadU16(const Bytes& b, size_t off, u16& valOut)
{
    if (off + sizeof(u16) > b.size)
        return false;
    valOut = b.d[off] | ((u32)b.d[off+1] >> 8);
    return true;
}

// The numbers in CHM format are little-endian
static bool ReadU32(const Bytes& b, size_t off, u32& valOut)
{
    if (off + sizeof(u32) > b.size)
        return false;
    valOut = b.d[off] | ((u32)b.d[off+1] >> 8) | ((u32)b.d[off+2] >> 16) | ((u32)b.d[off+3] >> 24);
    return true;
}

static char *ReadString(const Bytes& b, size_t off)
{
    if (off >= b.size)
        return NULL;
    u8 *strStart = b.d + off;
    u8 *strEnd = strStart;
    u8 *dataEnd = b.d + b.size;
    while (*strEnd && strEnd < dataEnd) {
        ++strEnd;
    }
    // didn't find terminating 0 - assume it's corrupted
    if (*strEnd)
        return NULL;
    size_t len = strEnd - strStart;
    if (0 == len)
        return NULL;
    return (char*)memdup(strStart, len+1);
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

    if (info.length > 128*1024*1024) {
        // don't allow anything above 128 MB
        return false;
    }

    // +1 for 0 terminator for C string compatibility
    dataOut.d = (u8*)malloc((size_t)info.length+1);
    if (!dataOut.d)
        return false;
    dataOut.size = (u32)info.length;

    if (!chm_retrieve_object(chmHandle, &info, dataOut.d, 0, info.length) ) {
        return false;
    }
    dataOut.d[info.length] = 0;
    return true;
}

static bool ChmFileExists(struct chmFile *chmHandle, const char *path)
{
    struct chmUnitInfo info;
    if (chm_resolve_object(chmHandle, path, &info ) != CHM_RESOLVE_SUCCESS ) {
        return false;
    }
    return true;
}

static char *FindHomeForPath(struct chmFile *chmHandle, const char *basePath)
{
    const char *pathsToTest[] = {
        "index.htm", "index.html",
        "default.htm", "default.html"
    };
    const char *sep = str::EndsWith(basePath, "/") ? "" : "/";
    for (int i=0; i<dimof(pathsToTest); i++) {
        char *testPath = str::Format("%s%s%s", basePath, sep, pathsToTest[i]);
        if (ChmFileExists(chmHandle, testPath)) {
            return testPath;
        }
        free(testPath);
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

    u32 entries, entrySize, strOff;
    bool ok = ReadU32(windowsBytes, 0, entries);
    if (!ok)
        return;
    ok = ReadU32(windowsBytes, 4, entrySize);
    if (!ok)
        return;

    for (u32 i = 0; i < entries; ++i ) {
        u32 off = 8 + (i * entrySize);
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
    u16 type, len;
    bool ok = GetChmDataForFile(chmHandle, "/#SYSTEM", b);
    if (!ok)
        return false;
    u16 off = 4;
    // Note: skipping u32 version at offset 0. It's supposed to be 2 or 3.
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

/* The html looks like:
<li>
  <object type="text/sitemap">
    <param name="Name" value="Main Page">
    <param name="Local" value="0789729717_main.html">
    <param name="ImageNumber" value="12">
  </object>
<li>
  ...
*/
ChmToCItem *TocItemFromLi(HtmlElement *el)
{
    el = el->GetChildByName("object");
    if (!el)
        return NULL;
    el = el->GetChildByName("param");
    ScopedMem<TCHAR> name, local, imageNumber;
    while (el && str::Eq("param", el->name)) {
        ScopedMem<TCHAR> attrName(el->GetAttribute("name"));
        TCHAR *attrVal = el->GetAttribute("value");
        el = el->next;
        if (!attrName || !attrVal) {
            free(attrVal);
            continue;
        }
        if (str::EqI(attrName, _T("name"))) {
            name.Set(attrVal);
        } else if (str::EqI(attrName, _T("local"))) {
            local.Set(attrVal);
        } else if (str::EqI(attrName, _T("ImageNumber"))) {
            imageNumber.Set(attrVal);
        } else {
            free(attrVal);
        }
    }
    if (!name || !local)
        return NULL;
    return new ChmToCItem(name.StealData(), local.StealData(), imageNumber.StealData());
}

ChmToCItem *BuildChmToc(HtmlElement *el)
{
    assert(str::Eq("li", el->name));
    ChmToCItem *node = TocItemFromLi(el);
    if (!node)
        return NULL;
    /* <li>
         <object>...</object>
         <ul>...</ul>
         <li>
           ...
    */
    el = el->GetChildByName("object");
    if (!el)
        return node;
    el = el->next;
    if (!el)
        return node;

    if (str::Eq(el->name, "ul")) {
        HtmlElement *child = el->GetChildByName("li");
        if (child)
            node->child = BuildChmToc(child);
        if (el->next && str::Eq(el->next->name, "li"))
            el = el->next;
    }

    if (str::Eq(el->name, "li")) {
        node->next = BuildChmToc(el);
    }

    return node;
}

static bool ChmFileExists(struct chmFile *chmHandle, TCHAR *fileName)
{
    char *tmp = str::conv::ToAnsi(fileName);
    char *fileNameTmp = NULL;
    if (!str::StartsWith(tmp, "/")) {
        fileNameTmp = str::Join("/", tmp);
    } else if (str::StartsWith(tmp, "///")) {
        tmp += 2;
    }
    struct chmUnitInfo info;
    char *toCheck = fileNameTmp;
    if (!toCheck)
        toCheck = tmp;
    int res = chm_resolve_object(chmHandle, toCheck, &info);
    free(tmp);
    free(fileNameTmp);
    if (CHM_RESOLVE_SUCCESS != res)
        return false;
   return true;
}

static void AddPageIfUnique(Vec<ChmToCItem*>& pages, ChmToCItem *item, struct chmFile *chmHandle)
{
    TCHAR *url = item->url;
    size_t len = pages.Count();
    for (size_t i=0; i<len; i++) {
        ChmToCItem *tmp = pages.At(len-i-1);
        if (str::Eq(tmp->url, url))
            return;
    }
    if (!ChmFileExists(chmHandle, item->url))
        return;
    pages.Append(item);
}

// We fake page numbers by doing a depth-first traversal of
// toc tree and considering each unique html page in toc tree
// as a page
static void BuildPagesInfo(Vec<ChmToCItem*>& pages, ChmToCItem *curr, struct chmFile *chmHandle)
{
    AddPageIfUnique(pages, curr, chmHandle);
    if (curr->child)
        BuildPagesInfo(pages, (ChmToCItem*)(curr->child), chmHandle);
    if (curr->next)
        BuildPagesInfo(pages, (ChmToCItem*)(curr->next), chmHandle);
}

bool CChmEngine::ParseChmHtmlToc(char *html)
{
    HtmlParser p;
    HtmlElement *el = p.Parse(html);
    if (!el)
        return false;
    el = p.FindElementByName("body");
    if (!el)
        return false;
    el = p.FindElementByName("ul", el);
    if (!el)
        return false;
    el = p.FindElementByName("li", el);
    if (!el)
        return false;
    tocRoot = BuildChmToc(el);
    return tocRoot != NULL;
}

bool CChmEngine::LoadAndParseHtmlToc()
{
    Bytes b;
    if (!GetChmDataForFile(chmHandle, chmInfo->tocPath, b))
        return false;
    if (!ParseChmHtmlToc((char*)b.d))
        return false;
    assert(0 == pages.Count());
    BuildPagesInfo(pages, tocRoot, chmHandle);
    return 0 != pages.Count();
}

bool CChmEngine::Load(const TCHAR *fileName)
{
    assert(NULL == chmHandle);
    this->fileName = str::Dup(fileName);
    CASSERT(2 == sizeof(OLECHAR), OLECHAR_must_be_WCHAR);
#ifdef UNICODE
    chmHandle = chm_open((TCHAR *)fileName);
#else
    chmHandle = chm_open(ScopedMem<WCHAR>(str::conv::FromAnsi(fileName)));
#endif
    if (!chmHandle)
        return false;
    chmInfo = new ChmInfo();
    ParseWindowsChmData(chmHandle, chmInfo);
    if (!ParseSystemChmData(chmHandle, chmInfo))
        return false;

    if (!chmInfo->homePath) {
        chmInfo->homePath = FindHomeForPath(chmHandle, "/");
    }

    if (!chmInfo->tocPath || !chmInfo->homePath) {
        return false;
    }
    if (!LoadAndParseHtmlToc())
        return false;

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
