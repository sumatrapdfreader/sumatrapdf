/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class PdfFilterState { Start, Author, Title, Date, Content, End };

class EngineBase;

class PdfFilter : public FilterBase
{
public:
    PdfFilter(long *plRefCount) : FilterBase(plRefCount) { }
    ~PdfFilter()  override { CleanUp(); }

    HRESULT OnInit() override;
    HRESULT GetNextChunkValue(ChunkValue &chunkValue) override;

    VOID CleanUp();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString(SZ_PDF_FILTER_HANDLER, pClassID);
    }

private:
    PdfFilterState m_state{PdfFilterState::End};
    int m_iPageNo{-1};
    EngineBase *m_pdfEngine{nullptr};
};
