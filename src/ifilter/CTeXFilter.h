/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"
#include "PdfFilter.h"

enum TEX_FILTER_STATE { STATE_TEX_START, STATE_TEX_PREAMBLE, STATE_TEX_CONTENT, STATE_TEX_END };

class CTeXFilter : public CFilterBase
{
public:
    CTeXFilter(long *plRefCount) : CFilterBase(plRefCount),
        m_state(STATE_TEX_END), m_pData(NULL), m_pPtr(NULL),
        m_pBuffer(NULL), m_iDepth(0) { }
    virtual ~CTeXFilter() { CleanUp(); }

    virtual HRESULT OnInit();
    virtual HRESULT GetNextChunkValue(CChunkValue &chunkValue);

    VOID CleanUp()
    {
        free(m_pData);
        m_pData = NULL;
        free(m_pBuffer);
        m_pBuffer = NULL;
    }
    WCHAR *ExtractBracedBlock();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromTString(SZ_TEX_FILTER_HANDLER, pClassID);
    }

private:
    TEX_FILTER_STATE m_state;
    WCHAR *m_pData, *m_pPtr, *m_pBuffer;
    int m_iDepth;
};
