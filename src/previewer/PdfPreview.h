/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfPreview_h
#define PdfPreview_h

#define SZ_PDF_PREVIEW_CLSID    _T("{3D3B1846-CC43-42ae-BFF9-D914083C2BA3}")
#ifdef BUILD_XPS_PREVIEW
#define SZ_XPS_PREVIEW_CLSID    _T("{D427A82C-6545-4fbe-8E87-030EDB3BE46D}")
#endif
#ifdef BUILD_CBZ_PREVIEW
#define SZ_CBZ_PREVIEW_CLSID    _T("{C29D3E2B-8FF6-4033-A4E8-54221D859D74}")
#endif
#ifdef BUILD_TGA_PREVIEW
#define SZ_TGA_PREVIEW_CLSID    _T("{CB1D63A6-FE5E-4ded-BEA5-3F6AF1A70D08}")
#endif

#include "BaseEngine.h"

#include <Thumbcache.h>

class PageRenderer;

class PreviewBase : public IThumbnailProvider, public IInitializeWithStream,
    public IObjectWithSite, public IPreviewHandler, public IOleWindow,
    // for Windows XP
    public IPersistFile, public IExtractImage2
{
public:
    PreviewBase(long *plRefCount, const TCHAR *clsid) : m_lRef(1),
        m_plModuleRef(plRefCount), m_pStream(NULL), m_engine(NULL),
        renderer(NULL), m_gdiScope(NULL), m_site(NULL), m_hwnd(NULL),
        m_hwndParent(NULL), m_clsid(clsid), m_extractCx(0) {
        InterlockedIncrement(m_plModuleRef);
        m_dateStamp.dwLowDateTime = m_dateStamp.dwHighDateTime = 0;
    }

    virtual ~PreviewBase() {
        Unload();
        delete m_gdiScope;
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        static const QITAB qit[] = {
            QITABENT(PreviewBase, IInitializeWithStream),
            QITABENT(PreviewBase, IThumbnailProvider),
            QITABENT(PreviewBase, IObjectWithSite),
            QITABENT(PreviewBase, IPreviewHandler),
            QITABENT(PreviewBase, IOleWindow),
            QITABENT(PreviewBase, IExtractImage2),
            QITABENT(PreviewBase, IExtractImage),
            QITABENT(PreviewBase, IPersistFile),
            QITABENT(PreviewBase, IPersist),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_lRef);
    }
    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha);

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pStm, DWORD grfMode) {
        m_pStream = pStm;
        if (!m_pStream)
            return E_INVALIDARG;
        m_pStream->AddRef();
        return S_OK;
    };

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown *punkSite) {
        m_site = NULL;
        if (!punkSite)
            return S_OK;
        return punkSite->QueryInterface(&m_site);
    }
    IFACEMETHODIMP GetSite(REFIID riid, void **ppv) {
        if (!ppv)
            return E_INVALIDARG;
        *ppv = NULL;
        if (!m_site)
            return E_FAIL;
        return m_site->QueryInterface(riid, ppv);
    }

    // IPreviewHandler
    IFACEMETHODIMP SetWindow(HWND hwnd, const RECT *prc) {
        if (!hwnd || !prc)
            return S_OK;
        m_hwndParent = hwnd;
        return SetRect(prc);
    }
    IFACEMETHODIMP SetFocus() {
        if (!m_hwnd)
            return S_FALSE;
        ::SetFocus(m_hwnd);
        return S_OK;
    }
    IFACEMETHODIMP QueryFocus(HWND *phwnd) {
        if (!phwnd) return E_INVALIDARG;
        *phwnd = GetFocus();
        if (!*phwnd)
            return HRESULT_FROM_WIN32(GetLastError());
        return S_OK;
    }
    IFACEMETHODIMP TranslateAccelerator(MSG *pmsg) {
        if (!m_site)
            return S_FALSE;
        ScopedComQIPtr<IPreviewHandlerFrame> frame(m_site);
        if (!frame)
            return S_FALSE;
        return frame->TranslateAccelerator(pmsg);
    }
    IFACEMETHODIMP SetRect(const RECT *prc) {
        if (!prc) return E_INVALIDARG;
        m_rcParent = RectI::FromRECT((RECT)*prc);
        if (m_hwnd) {
            SetWindowPos(m_hwnd, NULL, m_rcParent.x, m_rcParent.y, m_rcParent.dx, m_rcParent.dy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(m_hwnd, NULL, TRUE);
            UpdateWindow(m_hwnd);
        }
        return S_OK;
    }
    IFACEMETHODIMP DoPreview();
    IFACEMETHODIMP Unload() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = NULL;
        }
        m_pStream = NULL;
        delete m_engine;
        m_engine = NULL;
        return S_OK;
    }

    // IOleWindow
    IFACEMETHODIMP GetWindow(HWND *phwnd) {
        if (!m_hwndParent || !phwnd)
            return E_INVALIDARG;
        *phwnd = m_hwndParent;
        return S_OK;
    }
    IFACEMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) { return E_NOTIMPL; }

    // IPersist (for Windows XP)
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString(m_clsid, pClassID);
    }

    // IPersistFile (for Windows XP)
    IFACEMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode) {
        HANDLE hFile = CreateFileW(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            return E_INVALIDARG;
        DWORD size = GetFileSize(hFile, NULL), read;
        HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!data)
            return E_OUTOFMEMORY;
        BOOL ok = ReadFile(hFile, GlobalLock(data), size, &read, NULL);
        GlobalUnlock(data);
        GetFileTime(hFile, NULL, NULL, &m_dateStamp);
        CloseHandle(hFile);

        IStream *pStm;
        if (!ok || FAILED(CreateStreamOnHGlobal(data, TRUE, &pStm))) {
            GlobalFree(data);
            return E_FAIL;
        }
        HRESULT res = Initialize(pStm, 0);
        pStm->Release();
        return res;
    }
    IFACEMETHODIMP IsDirty(void) { return E_NOTIMPL; }
    IFACEMETHODIMP Save(LPCOLESTR pszFileName, BOOL bRemember) { return E_NOTIMPL; }
    IFACEMETHODIMP SaveCompleted(LPCOLESTR pszFileName) { return E_NOTIMPL; }
    IFACEMETHODIMP GetCurFile(LPOLESTR *ppszFileName) { return E_NOTIMPL; }

    // IExtractImage2 (for Windows XP)
    IFACEMETHODIMP Extract(HBITMAP *phBmpThumbnail) {
        if (!phBmpThumbnail || !m_extractCx)
            return E_INVALIDARG;
        WTS_ALPHATYPE dummy;
        return GetThumbnail(m_extractCx, phBmpThumbnail, &dummy);
    }
    IFACEMETHODIMP GetLocation(LPWSTR pszPathBuffer, DWORD cch, DWORD *pdwPriority, const SIZE *prgSize, DWORD dwRecClrDepth, DWORD *pdwFlags) {
        if (!prgSize || !pdwFlags)
            return E_INVALIDARG;
        // cheap implementation: ignore anything that isn't useful for IThumbnailProvider::GetThumbnail
        m_extractCx = min(prgSize->cx, prgSize->cy);
        *pdwFlags |= IEIFLAG_CACHE;
        return S_OK;
    }
    IFACEMETHODIMP GetDateStamp(FILETIME *pDateStamp) {
        if (!m_dateStamp.dwLowDateTime && !m_dateStamp.dwHighDateTime)
            return E_FAIL;
        *pDateStamp = m_dateStamp;
        return S_OK;
    }

    BaseEngine *GetEngine() {
        if (!m_engine && m_pStream)
            m_engine = LoadEngine(m_pStream);
        return m_engine;
    }

    PageRenderer *renderer;

protected:
    long m_lRef, * m_plModuleRef;
    ScopedComPtr<IStream> m_pStream;
    BaseEngine *m_engine;
    // engines based on ImagesEngine require GDI+ to be preloaded
    ScopedGdiPlus *m_gdiScope;
    // state for IPreviewHandler
    ScopedComPtr<IUnknown> m_site;
    HWND        m_hwnd, m_hwndParent;
    RectI       m_rcParent;
    // for IExtractImage2
    const TCHAR*m_clsid;
    UINT        m_extractCx;
    FILETIME    m_dateStamp;

    virtual BaseEngine *LoadEngine(IStream *stream) = 0;
};

class CPdfPreview : public PreviewBase {
public:
    CPdfPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_PDF_PREVIEW_CLSID) { }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};

#ifdef BUILD_XPS_PREVIEW
class CXpsPreview : public PreviewBase {
public:
    CXpsPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_XPS_PREVIEW_CLSID) { }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_CBZ_PREVIEW
class CCbzPreview : public PreviewBase {
public:
    CCbzPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_CBZ_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_TGA_PREVIEW
class CTgaPreview : public PreviewBase {
public:
    CTgaPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_TGA_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};
#endif

#endif
