/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum TEX_FILTER_STATE { STATE_TEX_START, STATE_TEX_PREAMBLE, STATE_TEX_CONTENT, STATE_TEX_END };

class CTeXFilter : public CFilterBase
{
public:
    CTeXFilter(long *plRefCount) : CFilterBase(plRefCount),
        m_state(STATE_TEX_END), m_pData(nullptr), m_pPtr(nullptr),
        m_pBuffer(nullptr), m_iDepth(0) { }
    ~CTeXFilter() override { CleanUp(); }

    HRESULT OnInit() override;
    HRESULT GetNextChunkValue(CChunkValue &chunkValue) override;

    VOID CleanUp()
    {
        free(m_pData);
        m_pData = nullptr;
        free(m_pBuffer);
        m_pBuffer = nullptr;
    }
    WCHAR *ExtractBracedBlock();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString(SZ_TEX_FILTER_HANDLER, pClassID);
    }

private:
    TEX_FILTER_STATE m_state;
    WCHAR *m_pData, *m_pPtr, *m_pBuffer;
    int m_iDepth;
};
