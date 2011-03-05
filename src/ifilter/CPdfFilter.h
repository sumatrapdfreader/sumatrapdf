/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

enum PDF_FILTER_STATE { STATE_PDF_START, STATE_PDF_AUTHOR, STATE_PDF_TITLE, STATE_PDF_DATE, STATE_PDF_CONTENT, STATE_PDF_END };

class PdfEngine;

class CPdfFilter : public CFilterBase
{
public:
    CPdfFilter(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount),
        m_state(STATE_PDF_END), m_iPageNo(-1), m_pdfEngine(NULL)
    {
        InterlockedIncrement(m_plModuleRef);
    }

    ~CPdfFilter()
    {
        CleanUp();
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] = {
            QITABENT(CPdfFilter, IPersistStream),
            QITABENT(CPdfFilter, IPersistFile),
            QITABENT(CPdfFilter, IInitializeWithStream),
            QITABENT(CPdfFilter, IFilter),
            { 0 }
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_lRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&m_lRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    virtual HRESULT OnInit();
    virtual HRESULT GetNextChunkValue(CChunkValue &chunkValue);

    VOID CleanUp();

private:
    long m_lRef, * m_plModuleRef;
	PDF_FILTER_STATE m_state;
    int m_iPageNo;
    PdfEngine *m_pdfEngine;
};
