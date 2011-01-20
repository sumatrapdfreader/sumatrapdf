/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

#define FILTER_TIMEOUT_IN_MS  1000

typedef struct {
    HANDLE produce, consume;
    char *data;
    size_t len;
} UpdateThreadData;

class CPdfFilter : public CFilterBase
{
public:
    CPdfFilter(long *plRefCount) : m_lRef(1), m_plModuleRef(plRefCount),
        m_hThread(NULL), m_bDone(false)
    {
        InterlockedIncrement(m_plModuleRef);
        ZeroMemory(&m_utd, sizeof(m_utd));
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
        if (m_utd.produce) {
            CloseHandle(m_utd.produce);
            m_utd.produce = NULL;
        }
        if (m_utd.consume) {
            CloseHandle(m_utd.consume);
            m_utd.consume = NULL;
        }
        if (m_hThread) {
            if (WaitForSingleObject(m_hThread, FILTER_TIMEOUT_IN_MS * 2) != WAIT_OBJECT_0) {
                TerminateThread(m_hThread, 99);
            }
            CloseHandle(m_hThread);
            m_hThread = NULL;
        }
        if (m_utd.data) {
            UnmapViewOfFile(m_utd.data);
            m_utd.data = NULL;
        }
        m_bDone = false;
    };

private:
    long m_lRef, * m_plModuleRef;

    UpdateThreadData m_utd;
    HANDLE m_hThread;
    bool m_bDone;
};
