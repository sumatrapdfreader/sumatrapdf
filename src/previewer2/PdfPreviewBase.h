/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <Thumbcache.h>

class PageRenderer;

class PreviewBase : public IThumbnailProvider,
                    public IInitializeWithStream,
                    public IObjectWithSite,
                    public IPreviewHandler,
                    public IOleWindow {
  public:
    PreviewBase(long* plRefCount, const char* clsid) {
        m_plModuleRef = plRefCount;
        InterlockedIncrement(m_plModuleRef);
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
    IFACEMETHODIMP Initialize(IStream* pStm, __unused DWORD grfMode) {
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
        m_rcParent = ToRect(*prc);
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
        if (m_engine) {
            m_engine->Release();
            m_engine = nullptr;
        }
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
    IFACEMETHODIMP ContextSensitiveHelp(__unused BOOL fEnterMode) {
        return E_NOTIMPL;
    }

    EngineBase* GetEngine() {
        if (!m_engine && m_pStream) {
            m_engine = LoadEngine(m_pStream);
        }
        return m_engine;
    }

    PageRenderer* renderer = nullptr;

  protected:
    long m_lRef = 1;
    long* m_plModuleRef = nullptr;
    ScopedComPtr<IStream> m_pStream;
    EngineBase* m_engine = nullptr;
    // engines based on EngineImages require GDI+ to be preloaded
    ScopedGdiPlus* m_gdiScope = nullptr;
    // state for IPreviewHandler
    ScopedComPtr<IUnknown> m_site;
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    Rect m_rcParent;

    virtual EngineBase* LoadEngine(IStream* stream) = 0;
};

class PdfPreview : public PreviewBase {
  public:
    PdfPreview(long* plRefCount) : PreviewBase(plRefCount, kPdfPreviewClsid) {
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

#if 0
class XpsPreview : public PreviewBase {
  public:
    XpsPreview(long* plRefCount) : PreviewBase(plRefCount, kXpsPreviewClsid) {
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};
#endif

class DjVuPreview : public PreviewBase {
  public:
    DjVuPreview(long* plRefCount) : PreviewBase(plRefCount, kDjVuPreviewClsid) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class EpubPreview : public PreviewBase {
  public:
    EpubPreview(long* plRefCount);
    ~EpubPreview();

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class Fb2Preview : public PreviewBase {
  public:
    Fb2Preview(long* plRefCount);
    ~Fb2Preview();

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class MobiPreview : public PreviewBase {
  public:
    MobiPreview(long* plRefCount);
    ~MobiPreview();

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class CbxPreview : public PreviewBase {
  public:
    CbxPreview(long* plRefCount) : PreviewBase(plRefCount, kCbxPreviewClsid) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};

class TgaPreview : public PreviewBase {
  public:
    TgaPreview(long* plRefCount) : PreviewBase(plRefCount, kTgaPreviewClsid) {
        m_gdiScope = new ScopedGdiPlus();
    }

  protected:
    EngineBase* LoadEngine(IStream* stream) override;
};
