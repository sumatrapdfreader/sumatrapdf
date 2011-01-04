/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

class CPdfFilter : public CFilterBase
{
public:
    CPdfFilter(long *plRefCount) : m_lRef(1), m_pData(NULL),
        m_plModuleRef(plRefCount), m_hMap(NULL), m_bSentType(false)
    {
        InterlockedIncrement(m_plModuleRef);
    }

    ~CPdfFilter()
    {
        if (m_pData)
            UnmapViewOfFile(m_pData);
        if (m_hMap)
            CloseHandle(m_hMap);
        InterlockedDecrement(m_plModuleRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] =
        {
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
        {
            delete this;
        }
        return cRef;
    }

    HRESULT OnInit();
    HRESULT GetNextChunkValue(CChunkValue &chunkValue);

private:
    long m_lRef, * m_plModuleRef;

    HANDLE m_hMap;
    char * m_pData;
    char * m_pSection;
    bool   m_bSentType;
};
