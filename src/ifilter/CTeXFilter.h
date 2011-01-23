/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

enum TEX_FILTER_STATE { STATE_TEX_START, STATE_TEX_PREAMBLE, STATE_TEX_CONTENT, STATE_TEX_END };

class CTeXFilter : public CFilterBase
{
public:
    CTeXFilter(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount), m_state(STATE_TEX_END), m_pData(NULL)
    {
        InterlockedIncrement(m_plModuleRef);
    }

    ~CTeXFilter()
    {
        CleanUp();
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] = {
            QITABENT(CTeXFilter, IPersistStream),
            QITABENT(CTeXFilter, IPersistFile),
            QITABENT(CTeXFilter, IInitializeWithStream),
            QITABENT(CTeXFilter, IFilter),
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

    HRESULT OnInit();
    HRESULT GetNextChunkValue(CChunkValue &chunkValue);

    VOID CleanUp()
    {
        if (m_pData)
        {
            free(m_pData);
            m_pData = NULL;
        }
        if (m_pBuffer)
        {
            free(m_pBuffer);
            m_pBuffer = NULL;
        }
    }
    WCHAR *ExtractBracedBlock();

private:
    long m_lRef, * m_plModuleRef;
    TEX_FILTER_STATE m_state;
    WCHAR *m_pData, *m_pPtr, *m_pBuffer;
    int m_iDepth;
};
