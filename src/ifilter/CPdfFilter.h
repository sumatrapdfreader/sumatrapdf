/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

class CPdfFilter : public CFilterBase
{
public:
    CPdfFilter(long *plRefCount) : m_lRef(1), m_pData(NULL),
        m_plModuleRef(plRefCount), m_hMap(NULL),
        m_hProcess(NULL), m_hProduce(NULL), m_hConsume(NULL),
        m_bDone(false)
#ifdef IFILTER_BUILTIN_MUPDF
        , m_pUniqueName(NULL)
#endif
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

    VOID CleanUp() {
        if (m_hProduce) {
            CloseHandle(m_hProduce);
            m_hProduce = NULL;
        }
        if (m_hConsume) {
            CloseHandle(m_hConsume);
            m_hConsume = NULL;
        }
        if (m_hProcess) {
            if (WaitForSingleObject(m_hProcess, FILTER_TIMEOUT_IN_MS * 2) != WAIT_OBJECT_0) {
#ifndef IFILTER_BUILTIN_MUPDF
                // don't let a stuck SumatraPDF.exe hang around
                TerminateProcess(m_hProcess, 99);
#else
                TerminateThread(m_hProcess, 99);
#endif
            }
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }
        if (m_pData) {
            UnmapViewOfFile(m_pData);
            m_pData = NULL;
        }
        if (m_hMap) {
            CloseHandle(m_hMap);
            m_hMap = NULL;
        }
#ifdef IFILTER_BUILTIN_MUPDF
        if (m_pUniqueName) {
            free(m_pUniqueName);
            m_pUniqueName = NULL;
        }
#endif
        m_bDone = false;
    };

private:
    long m_lRef, * m_plModuleRef;

    HANDLE m_hProcess, m_hMap, m_hProduce, m_hConsume;
    char * m_pData;
    bool m_bDone;
#ifdef IFILTER_BUILTIN_MUPDF
    VOID * m_pUniqueName;
#endif
};
