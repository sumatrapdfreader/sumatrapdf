/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "FilterBase.h"

enum TEX_FILTER_STATE { STATE_TEX_START, STATE_TEX_PREAMBLE, STATE_TEX_CONTENT, STATE_TEX_END };

class CTeXFilter : public CFilterBase
{
public:
    CTeXFilter(long *plRefCount) : CFilterBase(plRefCount),
        m_state(STATE_TEX_END), m_pData(NULL) { }
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

private:
    TEX_FILTER_STATE m_state;
    WCHAR *m_pData, *m_pPtr, *m_pBuffer;
    int m_iDepth;
};
