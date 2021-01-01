/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <Thumbcache.h>

class PageRenderer;

class PreviewBase : public IThumbnailProvider,
                    public IInitializeWithStream,
                    public IObjectWithSite,
                    public IPreviewHandler,
                    public IOleWindow,
                    // for Windows XP
                    public IPersistFile,
                    public IExtractImage2 {
  public:
    PreviewBase(long* plRefCount, const WCHAR* clsid) {
        m_plModuleRef = plRefCount;
        m_clsid = clsid;
        InterlockedIncrement(m_plModuleRef);
        m_dateStamp.dwLowDateTime = 0;
        m_dateStamp.dwHighDateTime = 0;
    }

    virtual ~PreviewBase() {
        Unload();
        delete m_gdiScope;
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(PreviewBase, IInitializeWithStream),
                                    QITABENT(PreviewBase, IThumbnailProvider),
                                    QITABENT(PreviewBase, IObjectWithSite),
                                    QITABENT(PreviewBase, IPreviewHandler),
                                    QITABENT(PreviewBase, IOleWindow),
                                    QITABENT(PreviewBase, IExtractImage2),
                                    QITABENT(PreviewBase, IExtractImage),
                                    QITABENT(PreviewBase, IPersistFile),
                                    QITABENT(PreviewBase, IPersist),
                                    {0}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_lRef);
    }
    IFACEMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(uint cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha);

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pStm, [[maybe_unused]] DWORD grfMode) {
        m_pStream = pStm;
        if (!m_pStream) {
            return E_INVALIDARG;
        }
        m_pStream->AddRef();
        return S_OK;
    };

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown* punkSite) {
        m_site = nullptr;
        if (!punkSite) {
            return S_OK;
        }
        return punkSite->QueryInterface(&m_site);
    }
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv) {
        if (m_site) {
            return m_site->QueryInterface(riid, ppv);
        }
        if (!ppv) {
            return E_INVALIDARG;
        }
        *ppv = nullptr;
        return E_FAIL;
    }

    // IPreviewHandler
    IFACEMETHODIMP SetWindow(HWND hwnd, const RECT* prc) {
        if (!hwnd || !prc) {
            return S_OK;
        }
        m_hwndParent = hwnd;
        return SetRect(prc);
    }
    IFACEMETHODIMP SetFocus() {
        if (!m_hwnd) {
            return S_FALSE;
        }
        ::SetFocus(m_hwnd);
        return S_OK;
    }
    IFACEMETHODIMP QueryFocus(HWND* phwnd) {
        if (!phwnd) {
            return E_INVALIDARG;
        }
        *phwnd = GetFocus();
        if (!*phwnd) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        return S_OK;
    }
    IFACEMETHODIMP TranslateAccelerator(MSG* pmsg) {
        if (!m_site) {
            return S_FALSE;
        }
        ScopedComQIPtr<IPreviewHandlerFrame> frame(m_site);
        if (!frame) {
            return S_FALSE;
        }
        return frame->TranslateAccelerator(pmsg);
    }
    IFACEMETHODIMP SetRect(const RECT* prc) {
        if (!prc) {
            return E_INVALIDARG;
        }
        m_rcParent = Rect::FromRECT(*prc);
        if (m_hwnd) {
            UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
            int x = m_rcParent.x;
            int y = m_rcParent.y;
            int dx = m_rcParent.dx;
            int dy = m_rcParent.dy;
            SetWindowPos(m_hwnd, nullptr, x, y, dx, dy, flags);
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
    IFACEMETHODIMP GetWindow(HWND* phwnd) {
        if (!m_hwndParent || !phwnd) {
            return E_INVALIDARG;
        }
        *phwnd = m_hwndParent;
        return S_OK;
    }
    IFACEMETHODIMP ContextSensitiveHelp([[maybe_unused]] BOOL fEnterMode) {
        return E_NOTIMPL;
    }

    // IPersist (for Windows XP)
    IFACEMETHODIMP GetClassID(CLSID* pClassID) {
        return CLSIDFromString((WCHAR*)m_clsid, pClassID);
    }

    // IPersistFile (for Windows XP)
    IFACEMETHODIMP Load(LPCOLESTR pszFileName, [[maybe_unused]] DWORD dwMode) {
        strconv::StackWstrToUtf8 fileName = pszFileName;
        dbglogf("PdfPreview: PreviewBase::Load('%s')\n", fileName.Get());

        HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            dbglog("PdfPreview: PreviewBase::Load() failed, no file\n");
            return E_INVALIDARG;
        }
        DWORD size = GetFileSize(hFile, nullptr), read;
        HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!data) {
            CloseHandle(hFile);
            dbglog("PdfPreview: PreviewBase::Load() failed, not enough memory\n");
            return E_OUTOFMEMORY;
        }
        BOOL ok = ReadFile(hFile, GlobalLock(data), size, &read, nullptr);
        GlobalUnlock(data);
        GetFileTime(hFile, nullptr, nullptr, &m_dateStamp);
        CloseHandle(hFile);

        IStream* pStm;
        if (!ok || FAILED(CreateStreamOnHGlobal(data, TRUE, &pStm))) {
            GlobalFree(data);
            dbglog("PdfPreview: PreviewBase::Load() failed, couldn't create stream\n");
            return E_FAIL;
        }
        HRESULT res = Initialize(pStm, 0);
        pStm->Release();
        return res;
    }
    IFACEMETHODIMP IsDirty() {
        return E_NOTIMPL;
    }
    IFACEMETHODIMP Save([[maybe_unused]] LPCOLESTR pszFileName, [[maybe_unused]] BOOL bRemember) {
        return E_NOTIMPL;
    }
    IFACEMETHODIMP SaveCompleted([[maybe_unused]] LPCOLESTR pszFileName) {
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetCurFile([[maybe_unused]] LPOLESTR* ppszFileName) {
        return E_NOTIMPL;
    }

    // IExtractImage2 (for Windows XP)
    IFACEMETHODIMP Extract(HBITMAP* phBmpThumbnail) {
        if (!phBmpThumbnail || !m_extractCx) {
            return E_INVALIDARG;
        }
        dbglog("PdfPreview: PreviewBase::Extract()\n");
        WTS_ALPHATYPE dummy;
        return GetThumbnail(m_extractCx, phBmpThumbnail, &dummy);
    }
    IFACEMETHODIMP GetLocation([[maybe_unused]] LPWSTR pszPathBuffer, [[maybe_unused]] DWORD cch,
                               [[maybe_unused]] DWORD* pdwPriority, const SIZE* prgSize,
                               [[maybe_unused]] DWORD dwRecClrDepth, DWORD* pdwFlags) {
        if (!prgSize || !pdwFlags) {
            return E_INVALIDARG;
        }
        dbglog("PdfPreview: PreviewBase::GetLocation()\n");
        // cheap implementation: ignore anything that isn't useful for IThumbnailProvider::GetThumbnail
        m_extractCx = std::min(prgSize->cx, prgSize->cy);
        *pdwFlags |= IEIFLAG_CACHE;
        return S_OK;
    }
    IFACEMETHODIMP GetDateStamp(FILETIME* pDateStamp) {
        if (!m_dateStamp.dwLowDateTime && !m_dateStamp.dwHighDateTime) {
            return E_FAIL;
        }
        dbglog("PdfPreview: PreviewBase::GetDateStamp()\n");
        *pDateStamp = m_dateStamp;
        return S_OK;
    }

    EngineBase* GetEngine() {
        if (!m_engine && m_pStream) {
            m_engine = LoadEngine(m_pStream);
        }
        return m_engine;
    }

    PageRenderer* renderer{nullptr};

  protected:
    long m_lRef{1};
    long* m_plModuleRef{nullptr};
    ScopedComPtr<IStream> m_pStream;
    EngineBase* m_engine{nullptr};
    // engines based on EngineImages require GDI+ to be preloaded
    ScopedGdiPlus* m_gdiScope{nullptr};
    // state for IPreviewHandler
    ScopedComPtr<IUnknown> m_site;
    HWND m_hwnd{nullptr};
    HWND m_hwndParent{nullptr};
    Rect m_rcParent;
    // for IExtractImage2
    const WCHAR* m_clsid{nullptr};
    uint m_extractCx{0};
    FILETIME m_dateStamp{0};

    virtual EngineBase* LoadEngine(IStream* stream) = 0;
};

class CPdfPreview : public PreviewBase {
  public:
    CPdfPreview(long* plRefCount) : PreviewBase(plRefCount, SZ_PDF_PREVIEW_CLSID) {
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CXpsPreview : public PreviewBase {
  public:
    CXpsPreview(long* plRefCount) : PreviewBase(plRefCount, SZ_XPS_PREVIEW_CLSID) {
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CDjVuPreview : public PreviewBase {
  public:
    CDjVuPreview(long* plRefCount) : PreviewBase(plRefCount, SZ_DJVU_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CEpubPreview : public PreviewBase {
  public:
    CEpubPreview(long* plRefCount);
    ~CEpubPreview();

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CFb2Preview : public PreviewBase {
  public:
    CFb2Preview(long* plRefCount);
    ~CFb2Preview();

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CMobiPreview : public PreviewBase {
  public:
    CMobiPreview(long* plRefCount);
    ~CMobiPreview();

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CCbxPreview : public PreviewBase {
  public:
    CCbxPreview(long* plRefCount) : PreviewBase(plRefCount, SZ_CBX_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CTgaPreview : public PreviewBase {
  public:
    CTgaPreview(long* plRefCount) : PreviewBase(plRefCount, SZ_TGA_PREVIEW_CLSID) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};
