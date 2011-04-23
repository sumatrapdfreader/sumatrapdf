/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

enum XPS_FILTER_STATE { STATE_XPS_START, STATE_XPS_AUTHOR, STATE_XPS_TITLE, STATE_XPS_DATE, STATE_XPS_CONTENT, STATE_XPS_END };

class XpsEngine;

class CXpsFilter : public CFilterBase
{
public:
    CXpsFilter(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount),
        m_state(STATE_XPS_END), m_iPageNo(-1), m_xpsEngine(NULL)
    {
        InterlockedIncrement(m_plModuleRef);
    }

    ~CXpsFilter()
    {
        CleanUp();
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] = {
            QITABENT(CXpsFilter, IPersistStream),
            QITABENT(CXpsFilter, IPersistFile),
            QITABENT(CXpsFilter, IInitializeWithStream),
            QITABENT(CXpsFilter, IFilter),
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
	XPS_FILTER_STATE m_state;
    int m_iPageNo;
    XpsEngine *m_xpsEngine;
};
