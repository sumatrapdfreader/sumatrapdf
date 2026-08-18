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
    PdfPreview(AtomicInt* plRefCount, PreviewType type);
    ~PdfPreview();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {QITABENT(PdfPreview, IInitializeWithStream),
                                    QITABENT(PdfPreview, IThumbnailProvider),
                                    QITABENT(PdfPreview, IObjectWithSite),
                                    QITABENT(PdfPreview, IPreviewHandler),
                                    QITABENT(PdfPreview, IOleWindow),
                                    {}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return AtomicIntInc(&m_lRef); }
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
        if (!pStm) {
            return E_INVALIDARG;
        }
        // The shell hands us a deny-write stream, so keeping it alive locks the
        // file for as long as the preview is on screen and an editor rebuilding
        // the document can't write over it (issue #1530). We only ever read it
        // once anyway - every engine is built from a memory buffer - so read it
        // here and let go of the file.
        m_data = ReadIStream(pStm);
        return str::IsNull(m_data) ? E_FAIL : S_OK;
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
            HwndInvalidate(m_hwnd, true);
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
        userZoom = 0;
        panX = 0;
        panY = 0;
        panning = false;
        str::FreePtr(&m_data);
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
        if (!m_engine && !str::IsNull(m_data)) {
            m_engine = LoadEngine(m_data);
            // the engine has its own copy; a failed load won't do better on a
            // second try, so let the bytes go either way
            str::FreePtr(&m_data);
        }
        return m_engine;
    }

    PageRenderer* renderer = nullptr;
    // 0 = fit page; otherwise engine-scale zoom (1 = 100%)
    float userZoom = 0.f;
    int panX = 0;
    int panY = 0;
    bool panning = false;
    Point panLast;

  protected:
    AtomicInt m_lRef = 1;
    AtomicInt* m_plModuleRef = nullptr;
    PreviewType m_type;
    // the file's bytes, owned; freed once the engine has been built from them
    Str m_data;
    EngineBase* m_engine = nullptr;
    ScopedGdiPlus* m_gdiScope = nullptr;
    // state for IPreviewHandler
    ScopedComPtr<IUnknown> m_site;
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    Rect m_rcParent;

    EngineBase* LoadEngine(const Str& data);
};
