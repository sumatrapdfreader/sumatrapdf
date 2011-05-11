/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfPreview_h
#define PdfPreview_h

#define SZ_PDF_PREVIEW_CLSID    L"{3D3B1846-CC43-42ae-BFF9-D914083C2BA3}"

#include "BaseUtil.h"
#include "PdfEngine.h"

#include <shlwapi.h>
#include <Thumbcache.h>

class PreviewBase : public IThumbnailProvider, public IInitializeWithStream
{
public:
    PreviewBase(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount),
        m_pStream(NULL), m_engine(NULL) {
        InterlockedIncrement(m_plModuleRef);
    }

    virtual ~PreviewBase() {
        if (m_pStream)
            m_pStream->Release();
        if (m_engine)
            delete m_engine;
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
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
        m_pStream->AddRef();

        m_engine = CreateEngineFromStream();
        if (!m_engine)
            return E_FAIL;
        return S_OK;
    };

protected:
    long m_lRef, * m_plModuleRef;
    IStream *   m_pStream;
    BaseEngine *m_engine;

    virtual BaseEngine *CreateEngineFromStream() = 0;
};

class CPdfPreview : public PreviewBase {
public:
    CPdfPreview(long *plRefCount) : PreviewBase(plRefCount) { }

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);

protected:
    virtual BaseEngine *CreateEngineFromStream() {
        return PdfEngine::CreateFromStream(m_pStream);
    }
};

#endif
