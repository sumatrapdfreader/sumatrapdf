/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum TEX_FILTER_STATE {
    STATE_TEX_START,
    STATE_TEX_PREAMBLE,
    STATE_TEX_CONTENT,
    STATE_TEX_END
};

class TeXFilter : public FilterBase {
  public:
    TeXFilter(AtomicInt* plRefCount)
        : FilterBase(plRefCount),
          m_state(STATE_TEX_END),
          m_pData(nullptr),
          m_pPtr(nullptr),
          m_pBuffer(nullptr),
          m_iDepth(0) {}
    ~TeXFilter() override { CleanUp(); }

    HRESULT OnInit() override;
    HRESULT GetNextChunkValue(ChunkValue& chunkValue) override;

    VOID CleanUp() {
        free(m_pData);
        m_pData = nullptr;
        m_pPtr = nullptr;
        free(m_pBuffer);
        m_pBuffer = nullptr;
        m_state = STATE_TEX_END;
        m_iDepth = 0;
    }
    WStr ExtractBracedBlock();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID* pClassID) override { return CLSIDFromString(StrL(kTexFilterClsid), pClassID); }

  private:
    TEX_FILTER_STATE m_state;
    WCHAR *m_pData, *m_pPtr, *m_pBuffer;
    int m_iDepth;
};
