/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfPreview_h
#define PdfPreview_h

#define SZ_PDF_PREVIEW_CLSID    _T("{3D3B1846-CC43-42ae-BFF9-D914083C2BA3}")

#include "BaseUtil.h"
#include "WinUtil.h"
#include "PdfEngine.h"

#include <shlwapi.h>
#include <Thumbcache.h>

class PageRenderer;

class PreviewBase : public IThumbnailProvider, public IInitializeWithStream,
    public IObjectWithSite, public IPreviewHandler, public IOleWindow
{
public:
    PreviewBase(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount),
        m_pStream(NULL), m_engine(NULL), renderer(NULL),
        m_site(NULL), m_hwnd(NULL), m_hwndParent(NULL) {
        InterlockedIncrement(m_plModuleRef);
    }

    virtual ~PreviewBase() {
        Unload();
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
        const IID *iids[] = {
            &IID_IInitializeWithStream,
            &IID_IThumbnailProvider,
            &IID_IObjectWithSite,
            &IID_IPreviewHandler,
            &IID_IOleWindow,
            NULL
        };
        return QIImpl(this, iids, riid, ppv);
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
        if (m_pStream)
            m_pStream->Release();
        m_pStream = pStm;
        if (!m_pStream) return E_INVALIDARG;
        m_pStream->AddRef();
        return S_OK;
    };

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown *punkSite) {
        if (m_site) {
            m_site->Release();
            m_site = NULL;
        }
        if (!punkSite)
            return S_OK;
        return punkSite->QueryInterface(&m_site);
    }
    IFACEMETHODIMP GetSite(REFIID riid, void **ppv) {
        if (!ppv) return E_INVALIDARG;
        *ppv = NULL;
        return m_site ? m_site->QueryInterface(riid, ppv) : E_FAIL;
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
        IPreviewHandlerFrame *frame;
        if (!m_site || FAILED(m_site->QueryInterface(&frame)))
            return S_FALSE;
        HRESULT hr = frame->TranslateAccelerator(pmsg);
        frame->Release();
        return hr;
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
        if (m_pStream) {
            m_pStream->Release();
            m_pStream = NULL;
        }
        delete m_engine;
        m_engine = NULL;
        return S_OK;
    }

    // IOleWindow
    IFACEMETHODIMP GetWindow(HWND *phwnd) {
        if (!m_hwndParent || !phwnd) return E_INVALIDARG;
        *phwnd = m_hwndParent;
        return S_OK;
    }
    IFACEMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) { return E_NOTIMPL; }

    BaseEngine *GetEngine() {
        if (!m_engine && m_pStream)
            m_engine = LoadEngine(m_pStream);
        return m_engine;
    }

    PageRenderer *renderer;

protected:
    long m_lRef, * m_plModuleRef;
    IStream *   m_pStream;
    BaseEngine *m_engine;
    // state for IPreviewHandler
    IUnknown *  m_site;
    HWND        m_hwnd, m_hwndParent;
    RectI       m_rcParent;

    virtual BaseEngine *LoadEngine(IStream *stream) = 0;
};

class CPdfPreview : public PreviewBase {
public:
    CPdfPreview(long *plRefCount) : PreviewBase(plRefCount) { }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream) {
        return PdfEngine::CreateFromStream(stream);
    }
};

#endif
