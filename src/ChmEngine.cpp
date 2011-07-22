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

    ChmToCItem(TCHAR *title, TCHAR *url, TCHAR *imageNumber) : DocToCItem(title)
    {
        this->url = url;
        this->imageNumber = imageNumber;
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
    virtual int PageCount() const { return 1 /* TODO: pageCount */; }

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

    int pageCount;

    bool Load(const TCHAR *fileName);
    bool LoadAndParseHtmlToc();
    bool ParseChmHtmlToc(char *html);
};

CChmEngine::CChmEngine() :
    fileName(NULL), chmHandle(NULL), chmInfo(NULL), tocRoot(NULL), pageCount(0)
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

A good html parser would treat param elements as siblings,
but our simplistic parser treats them as children.
*/
ChmToCItem *TocItemFromLi(HtmlElement *el)
{
    el = el->GetChildIfNamed(0, "object");
    if (!el)
        return NULL;
    el = el->GetChildIfNamed(0, "param");
    TCHAR *name = NULL;
    TCHAR *local = NULL;
    TCHAR *imageNumber = NULL;
    while (el && str::Eq("param", el->name)) {
        ScopedMem<TCHAR> attrName(el->GetAttribute("name"));
        TCHAR *attrVal = el->GetAttribute("value");
        el = el->down;
        if (!attrName || !attrVal)
            continue;
        if (str::EqI(attrName, _T("name"))) {
            name = attrVal;
        } else if (str::EqI(attrName, _T("local"))) {
            local = attrVal;
        } else if (str::EqI(attrName, _T("ImageNumber"))) {
            imageNumber = attrVal;
        } else {
            free(attrVal);
        }
    }
    if (!name || !local) {
        free(name);
        free(local);
        free(imageNumber);
        return NULL;
    }
    return new ChmToCItem(name, local, imageNumber);
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
    el = el->GetChildIfNamed(0, "object");
    if (!el)
        return node;
    el = el->next;
    if (!el)
        return node;

    if (str::Eq(el->name, "ul")) {
        HtmlElement *child = el->GetChildIfNamed(0, "li");
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
    bool ok = GetChmDataForFile(chmHandle, chmInfo->tocPath, b);
    if (ok)
        ok = ParseChmHtmlToc((char*)b.d);
    return ok;
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

    // TODO: build pages information. Really, we have to fake the pages
    // as they are not a native concept to chm document.
    // We'll construct pages information by traversing toc tree
    // depth-first, assigning each unique, existing html page a 
    // consequitive page number
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
