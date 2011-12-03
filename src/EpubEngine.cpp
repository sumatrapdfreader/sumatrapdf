/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#pragma warning( push )
#pragma warning( disable: 4018 )
#include <crengine.h>
#pragma warning( pop )

#include "EpubEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Scopes.h"

class CREngineCtx {
    int count;
    CRITICAL_SECTION access;

public:
    CREngineCtx() : count(0) { InitializeCriticalSection(&access); }
    ~CREngineCtx() {
        assert(count == 0);
        DeleteCriticalSection(&access);
    }

    void Lock() { EnterCriticalSection(&access); }
    void Unlock() { LeaveCriticalSection(&access); }

    void Initialize() {
        ScopedCritSec scope(&access);
        if (count == 0)
            InitFontManager(lString8());
        count++;
    }
    void Uninitialize() {
        ScopedCritSec scope(&access);
        count--;
        if (count == 0)
            ShutdownFontManager();
    }
};
static CREngineCtx gEngineCtx;

class CEpubEngine : public EpubEngine {
    friend EpubEngine;

public:
    CEpubEngine();
    virtual ~CEpubEngine();
    virtual EpubEngine *Clone() {
        return CreateFromFileName(fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return docView->getPageList()->length(); }

    virtual RectD PageMediabox(int pageNo) { return pageRect; }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        return PageMediabox(pageNo);
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View);

    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
        LVColorDrawBuf drawBuf(600, 800);
        gEngineCtx.Lock();
        docView->SetPos(docView->getPageList()->get(pageNo - 1)->start, false);
        docView->Draw(drawBuf);
        gEngineCtx.Unlock();
        drawBuf.DrawTo(hDC, 0, 0, 0, NULL);
        return true;
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
        // docView->getPageText(...)
        return NULL;
    }

    virtual bool IsImagePage(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    virtual bool HasTocTree() const {
        // return docView->getToc() != NULL;
        return false;
    }

    virtual const TCHAR *GetDefaultFileExt() const { return path::GetExt(fileName); }

    virtual bool BenchLoadPage(int pageNo) { return true; }

protected:
    const TCHAR *fileName;
    LVDocView *docView;
    RectD pageRect;

    bool Load(const TCHAR *fileName);
};

CEpubEngine::CEpubEngine() : fileName(NULL), docView(NULL),
    pageRect(0, 0, 600, 800)
{
}

CEpubEngine::~CEpubEngine()
{
    if (docView) {
        delete docView;
        gEngineCtx.Uninitialize();
    }
    free((void *)fileName);
}

bool CEpubEngine::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    gEngineCtx.Initialize();
    docView = new LVDocView();
    if (!docView) {
        gEngineCtx.Uninitialize();
        return false;
    }

    bool ok = docView->LoadDocument(AsWStrQ(fileName));
    if (!ok)
        return false;

    docView->setPageMargins(lvRect(10, 10, 10, 10));
    docView->setPageHeaderInfo(PGHDR_NONE);
    docView->setViewMode(DVM_PAGES);
    docView->Resize((int)pageRect.dx, (int)pageRect.dy);
    docView->Render();

    return true;
}

RenderedBitmap *CEpubEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, screen, pageNo, zoom, rotation, pageRect, target);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!ok) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.Size());
}

unsigned char *CEpubEngine::GetFileData(size_t *cbCount)
{
    return (unsigned char *)file::ReadAll(fileName, cbCount);
}

bool EpubEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, _T(".epub")) ||
           str::EndsWithI(fileName, _T(".fb2"));
}

EpubEngine *EpubEngine::CreateFromFileName(const TCHAR *fileName)
{
    CEpubEngine *engine = new CEpubEngine();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
