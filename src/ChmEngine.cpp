/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ChmEngine.h"
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

    // DPI isn't constant for all pages and thus premultiplied
    virtual float GetFileDPI() const { return 300.0f; }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".chm"); }

    // we currently don't load pages lazily, so there's nothing to do here
    virtual bool BenchLoadPage(int pageNo) { return true; }

    // TODO: for now, it obviously has toc tree
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

static bool GetChmDataForFile(struct chmFile *chmHandle, const char *fileNameIn, str::Str<char>& dataOut)
{
    bool ok = false;
    const char *fileNameTmp = NULL;
    if (str::StartsWith(fileNameIn, "/")) {
        if (str::StartsWith(fileNameIn, "///")) {
            fileNameIn = fileNameIn + 2;
        }
    } else {
        fileNameTmp = str::Join("/", fileNameIn);
    }
    const char *fileName = fileNameTmp ? fileNameTmp : fileNameIn;

    struct chmUnitInfo info;
    int res = chm_resolve_object(chmHandle, fileName, &info);
    if (CHM_RESOLVE_SUCCESS != res) {
        goto Exit;
    }

    if (info.length > 128*1024*1024) {
        // don't allow anything above 128 MB
        goto Exit;
    }

    dataOut.Reset();
    unsigned char *buf = (unsigned char*)dataOut.MakeSpaceAt(0, (size_t)info.length);
    if (!buf)
        goto Exit;

    if (!chm_retrieve_object(chmHandle, &info, buf, 0, info.length) ) {
        goto Exit;
    }

Exit:
    free((void*)fileNameTmp);
    return ok;
}

bool CChmEngine::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);
    CASSERT(2 == sizeof(OLECHAR), OLECHAR_must_be_WCHAR);
    chmHandle = chm_open((BSTR)fileName);
    if (!chmHandle)
        return false;
    str::Str<char> windowsData;
    str::Str<char> stringsData;
    bool hasWindows = GetChmDataForFile(chmHandle, "/#WINDOWS", windowsData);
    bool hasStrings = GetChmDataForFile(chmHandle, "/#STRINGS", windowsData);
    if (hasWindows && hasStrings) {
        // TODO: write me

    }
    // TODO: write me
    return false;
}

unsigned char *CChmEngine::GetFileData(size_t *cbCount)
{
    return (unsigned char *)file::ReadAll(fileName, cbCount);
}

bool ChmEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
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
