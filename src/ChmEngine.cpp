/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Vec.h"

#include "chm_lib.h"

class CChmEngine : public ChmEngine {
    friend ChmEngine;

public:
    CChmEngine();
    virtual ~CChmEngine();
    virtual ChmEngine *Clone() {
        return CreateFromFileName(fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pageCount; }

    virtual RectD PageMediabox(int pageNo) {
        RectD r; return r;
    }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        RectD r; return r;
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View) {
         assert(0);
         return NULL;
    }

    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
        assert(0);
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

    // we currently don't load pages lazily, so there's nothing to do here
    virtual bool BenchLoadPage(int pageNo) { return true; }

    // TODO: for now, it obviously has no toc tree
    virtual bool HasToCTree() const { return false; }

protected:
    const TCHAR *fileName;
    struct chmFile *chmHandle;

    int pageCount;

    bool Load(const TCHAR *fileName);
};

CChmEngine::CChmEngine() :
    fileName(NULL), chmHandle(NULL), pageCount(0)
{
}

CChmEngine::~CChmEngine()
{
    chm_close(chmHandle);
    free((void *)fileName);
}

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

CASSERT(2 == sizeof(u16), u16_is_2_bytes);
CASSERT(4 == sizeof(u32), u32_is_4_bytes);

struct Bytes {
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
    return (char*)memdup(strStart, len);
}

static char *ReadWsTrimmedString(const Bytes& b, size_t off)
{
    char *s = ReadString(b, off);
    if (s) {
        str::RemoveChars(s, "\n\r\t");
    }
    return s;
}

// Data parsed from /#WINDOWS and /#STRINGS files inside CHM
class WindowsChmData {
public:
    WindowsChmData() : title(NULL), tocPath(NULL), indexPath(NULL), homePath(NULL)
    {}
    ~WindowsChmData() {
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

// http://www.nongnu.org/chmspec/latest/Internal.html#WINDOWS
static WindowsChmData* ParseWindowsChmData(str::Str<char>& windowsData, str::Str<char>& stringsData)
{
    Bytes windowsBytes = { (u8*)windowsData.LendData(), windowsData.Count() };
    Bytes stringsBytes = { (u8*)stringsData.LendData(), stringsData.Count() };
    u32 entries, entrySize, strOff;
    bool ok = ReadU32(windowsBytes, 0, entries);
    if (!ok)
        return NULL;
    ok = ReadU32(windowsBytes, 4, entrySize);
    if (!ok)
        return NULL;

    WindowsChmData *wd = new WindowsChmData();
    for (u32 i = 0; i < entries; ++i ) {
        u32 off = 8 + (i * entrySize);
        if (!wd->title) {
            ok = ReadU32(windowsBytes, off + 0x14, strOff);
            if (ok) {
                wd->title = ReadWsTrimmedString(stringsBytes, strOff);
            }
        }
        if (!wd->tocPath) {
            ok = ReadU32(windowsBytes, off + 0x60, strOff);
            if (ok) {
                wd->tocPath = ReadString(stringsBytes, strOff);
            }
        }
        if (!wd->indexPath) {
            ok = ReadU32(windowsBytes, off + 0x64, strOff);
            if (ok) {
                wd->indexPath = ReadString(stringsBytes, strOff);
            }
        }
        if (!wd->homePath) {
            ok = ReadU32(windowsBytes, off+0x68, strOff);
            if (ok) {
                wd->homePath = ReadString(stringsBytes, strOff);
            }
        }
    }
    return wd;
}

static bool GetChmDataForFile(struct chmFile *chmHandle, const char *fileName, str::Str<char>& dataOut)
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

    dataOut.Reset();
    unsigned char *buf = (unsigned char*)dataOut.MakeSpaceAt(0, (size_t)info.length);
    if (!buf)
        return false;

    if (!chm_retrieve_object(chmHandle, &info, buf, 0, info.length) ) {
        return false;
    }

    return true;
}

bool CChmEngine::Load(const TCHAR *fileName)
{
    WindowsChmData *wd = NULL;
    this->fileName = str::Dup(fileName);
    CASSERT(2 == sizeof(OLECHAR), OLECHAR_must_be_WCHAR);
#ifdef UNICODE
    chmHandle = chm_open((TCHAR *)fileName);
#else
    chmHandle = chm_open(ScopedMem<WCHAR>(str::conv::FromAnsi(fileName)));
#endif
    if (!chmHandle)
        return false;
    str::Str<char> windowsData;
    str::Str<char> stringsData;
    bool hasWindows = GetChmDataForFile(chmHandle, "/#WINDOWS", windowsData);
    bool hasStrings = GetChmDataForFile(chmHandle, "/#STRINGS", stringsData);
    if (hasWindows && hasStrings) {
        wd = ParseWindowsChmData(windowsData, stringsData);
    }
    // TODO: write me
    delete wd; // temporary
    return false;
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
