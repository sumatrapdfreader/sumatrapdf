/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
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
#ifdef BUILD_EPUB_PREVIEW
#define SZ_EPUB_PREVIEW_CLSID   _T("{80C4E4B1-2B0F-40d5-95AF-BE7B57FEA4F9}")
#endif

#include "BaseEngine.h"

#include <Thumbcache.h>

class PageRenderer;

class PreviewBase : public IThumbnailProvider, public IInitializeWithStream,
    public IObjectWithSite, public IPreviewHandler, public IOleWindow
{
public:
    PreviewBase(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount),
        m_pStream(NULL), m_engine(NULL), renderer(NULL), m_gdiScope(NULL),
        m_site(NULL), m_hwnd(NULL), m_hwndParent(NULL) {
        InterlockedIncrement(m_plModuleRef);
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

    virtual BaseEngine *LoadEngine(IStream *stream) = 0;
};

class CPdfPreview : public PreviewBase {
public:
    CPdfPreview(long *plRefCount) : PreviewBase(plRefCount) { }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};

#ifdef BUILD_XPS_PREVIEW
class CXpsPreview : public PreviewBase {
public:
    CXpsPreview(long *plRefCount) : PreviewBase(plRefCount) { }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_CBZ_PREVIEW
class CCbzPreview : public PreviewBase {
public:
    CCbzPreview(long *plRefCount) : PreviewBase(plRefCount) {
        m_gdiScope = new ScopedGdiPlus();
    }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};
#endif

#ifdef BUILD_EPUB_PREVIEW
class CEpubPreview : public PreviewBase {
public:
    CEpubPreview(long *plRefCount) : PreviewBase(plRefCount) {
        m_gdiScope = new ScopedGdiPlus();
    }

protected:
    virtual BaseEngine *LoadEngine(IStream *stream);
};
#endif

#endif
