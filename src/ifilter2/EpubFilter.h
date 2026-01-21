/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum EPUB_FILTER_STATE {
    STATE_EPUB_START,
    STATE_EPUB_AUTHOR,
    STATE_EPUB_TITLE,
    STATE_EPUB_DATE,
    STATE_EPUB_CONTENT,
    STATE_EPUB_END
};

// Forward declaration - defined in PdfFilter.h
struct IFilterExtractedData;

class EpubFilter : public FilterBase {
  public:
    EpubFilter(long* plRefCount) : FilterBase(plRefCount), m_state(STATE_EPUB_END), m_extractedData(nullptr) {
    }
    ~EpubFilter() override {
        CleanUp();
    }

    HRESULT OnInit() override;
    HRESULT GetNextChunkValue(ChunkValue& chunkValue) override;

    VOID CleanUp();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID* pClassID) {
        return CLSIDFromString(kEpubFilterHandler, pClassID);
    }

  private:
    EPUB_FILTER_STATE m_state;
    IFilterExtractedData* m_extractedData;
    int m_iPageNo = 0;
};
