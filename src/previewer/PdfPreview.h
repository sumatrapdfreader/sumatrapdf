/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <thumbcache.h>

class PageRenderer;

enum class PreviewType {
    Pdf,
    Xps,
    DjVu,
    Epub,
    Fb2,
    Mobi,
    Cbx,
    Tga,
};

class PdfPreview : public IThumbnailProvider,
                   public IInitializeWithStream,
                   public IObjectWithSite,
                   public IPreviewHandler,
                   public IOleWindow {
  public:
    PdfPreview(long* plRefCount, PreviewType type);
    ~PdfPreview();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(PdfPreview, IInitializeWithStream),
                                    QITABENT(PdfPreview, IThumbnailProvider),
                                    QITABENT(PdfPreview, IObjectWithSite),
                                    QITABENT(PdfPreview, IPreviewHandler),
                                    QITABENT(PdfPreview, IOleWindow),
                                    {0}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_lRef); }
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
    IFACEMETHODIMP ContextSensitiveHelp(__unused BOOL fEnterMode) { return E_NOTIMPL; }

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
    PreviewType m_type;
    ScopedComPtr<IStream> m_pStream;
    EngineBase* m_engine = nullptr;
    ScopedGdiPlus* m_gdiScope = nullptr;
    bool m_muiInitialized = false;
    // state for IPreviewHandler
    ScopedComPtr<IUnknown> m_site;
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    Rect m_rcParent;

    EngineBase* LoadEngine(IStream* stream);
};
