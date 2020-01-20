/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <Thumbcache.h>

class PageRenderer;

class PreviewBase : public IThumbnailProvider, public IInitializeWithStream,
    public IObjectWithSite, public IPreviewHandler, public IOleWindow,
    // for Windows XP
    public IPersistFile, public IExtractImage2
{
public:
    PreviewBase(long *plRefCount, const WCHAR *clsid) : m_lRef(1),
        m_plModuleRef(plRefCount), m_pStream(nullptr), m_engine(nullptr),
        renderer(nullptr), m_gdiScope(nullptr), m_site(nullptr), m_hwnd(nullptr),
        m_hwndParent(nullptr), m_clsid(clsid), m_extractCx(0) {
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
        UNUSED(grfMode);
        m_pStream = pStm;
        if (!m_pStream)
            return E_INVALIDARG;
        m_pStream->AddRef();
        return S_OK;
    };

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown *punkSite) {
        m_site = nullptr;
        if (!punkSite)
            return S_OK;
        return punkSite->QueryInterface(&m_site);
    }
    IFACEMETHODIMP GetSite(REFIID riid, void **ppv) {
        if (m_site)
            return m_site->QueryInterface(riid, ppv);
        if (!ppv)
            return E_INVALIDARG;
        *ppv = nullptr;
        return E_FAIL;
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
        m_rcParent = RectI::FromRECT(*prc);
        if (m_hwnd) {
            SetWindowPos(m_hwnd, nullptr, m_rcParent.x, m_rcParent.y, m_rcParent.dx, m_rcParent.dy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(m_hwnd, nullptr, TRUE);
            UpdateWindow(m_hwnd);
        }
        return S_OK;
    }
    IFACEMETHODIMP DoPreview();
    IFACEMETHODIMP Unload() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        m_pStream = nullptr;
        delete m_engine;
        m_engine = nullptr;
        return S_OK;
    }

    // IOleWindow
    IFACEMETHODIMP GetWindow(HWND *phwnd) {
        if (!m_hwndParent || !phwnd)
            return E_INVALIDARG;
        *phwnd = m_hwndParent;
        return S_OK;
    }
    IFACEMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) { UNUSED(fEnterMode); return E_NOTIMPL; }

    // IPersist (for Windows XP)
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString((WCHAR *)m_clsid, pClassID);
    }

    // IPersistFile (for Windows XP)
    IFACEMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode) {
        UNUSED(dwMode);
        HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return E_INVALIDARG;
        DWORD size = GetFileSize(hFile, nullptr), read;
        HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!data) {
            CloseHandle(hFile);
            return E_OUTOFMEMORY;
        }
        BOOL ok = ReadFile(hFile, GlobalLock(data), size, &read, nullptr);
        GlobalUnlock(data);
        GetFileTime(hFile, nullptr, nullptr, &m_dateStamp);
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
    IFACEMETHODIMP IsDirty() { return E_NOTIMPL; }
    IFACEMETHODIMP Save(LPCOLESTR pszFileName, BOOL bRemember) { UNUSED(pszFileName); UNUSED(bRemember); return E_NOTIMPL; }
    IFACEMETHODIMP SaveCompleted(LPCOLESTR pszFileName) { UNUSED(pszFileName);  return E_NOTIMPL; }
    IFACEMETHODIMP GetCurFile(LPOLESTR *ppszFileName) { UNUSED(ppszFileName); return E_NOTIMPL; }

    // IExtractImage2 (for Windows XP)
    IFACEMETHODIMP Extract(HBITMAP *phBmpThumbnail) {
        if (!phBmpThumbnail || !m_extractCx)
            return E_INVALIDARG;
        WTS_ALPHATYPE dummy;
        return GetThumbnail(m_extractCx, phBmpThumbnail, &dummy);
    }
    IFACEMETHODIMP GetLocation(LPWSTR pszPathBuffer, DWORD cch, DWORD *pdwPriority, const SIZE *prgSize, DWORD dwRecClrDepth, DWORD *pdwFlags) {
        UNUSED(pszPathBuffer);  UNUSED(cch);  UNUSED(pdwPriority); UNUSED(dwRecClrDepth);
        if (!prgSize || !pdwFlags)
            return E_INVALIDARG;
        // cheap implementation: ignore anything that isn't useful for IThumbnailProvider::GetThumbnail
        m_extractCx = std::min(prgSize->cx, prgSize->cy);
        *pdwFlags |= IEIFLAG_CACHE;
        return S_OK;
    }
    IFACEMETHODIMP GetDateStamp(FILETIME *pDateStamp) {
        if (!m_dateStamp.dwLowDateTime && !m_dateStamp.dwHighDateTime)
            return E_FAIL;
        *pDateStamp = m_dateStamp;
        return S_OK;
    }

    EngineBase *GetEngine() {
        if (!m_engine && m_pStream)
            m_engine = LoadEngine(m_pStream);
        return m_engine;
    }

    PageRenderer *renderer;

protected:
    long m_lRef, * m_plModuleRef;
    ScopedComPtr<IStream> m_pStream;
    EngineBase *m_engine;
    // engines based on EngineImages require GDI+ to be preloaded
    ScopedGdiPlus *m_gdiScope;
    // state for IPreviewHandler
    ScopedComPtr<IUnknown> m_site;
    HWND        m_hwnd, m_hwndParent;
    RectI       m_rcParent;
    // for IExtractImage2
    const WCHAR*m_clsid;
    UINT        m_extractCx;
    FILETIME    m_dateStamp;

    virtual EngineBase *LoadEngine(IStream *stream) = 0;
};

class CPdfPreview : public PreviewBase {
public:
    CPdfPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_PDF_PREVIEW_CLSID) { }

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};

#ifdef BUILD_XPS_PREVIEW
class CXpsPreview : public PreviewBase {
public:
    CXpsPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_XPS_PREVIEW_CLSID) { }

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_DJVU_PREVIEW
class CDjVuPreview : public PreviewBase {
public:
    CDjVuPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_DJVU_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_EPUB_PREVIEW
class CEpubPreview : public PreviewBase {
public:
    CEpubPreview(long *plRefCount);
    ~CEpubPreview();

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_FB2_PREVIEW
class CFb2Preview : public PreviewBase {
public:
    CFb2Preview(long *plRefCount);
    ~CFb2Preview();

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_MOBI_PREVIEW
class CMobiPreview : public PreviewBase {
public:
    CMobiPreview(long *plRefCount);
    ~CMobiPreview();

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};
#endif

#if defined(BUILD_CBZ_PREVIEW) || defined(BUILD_CBR_PREVIEW) || defined(BUILD_CB7_PREVIEW) || defined(BUILD_CBT_PREVIEW)
class CCbxPreview : public PreviewBase {
public:
    CCbxPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_CBX_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_TGA_PREVIEW
class CTgaPreview : public PreviewBase {
public:
    CTgaPreview(long *plRefCount) : PreviewBase(plRefCount, SZ_TGA_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

protected:
    virtual EngineBase *LoadEngine(IStream *stream);
};
#endif
