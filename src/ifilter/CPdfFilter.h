/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum PDF_FILTER_STATE { STATE_PDF_START, STATE_PDF_AUTHOR, STATE_PDF_TITLE, STATE_PDF_DATE, STATE_PDF_CONTENT, STATE_PDF_END };

class EngineBase;

class PdfFilter : public FilterBase
{
public:
    PdfFilter(long *plRefCount) : FilterBase(plRefCount),
        m_state(STATE_PDF_END), m_iPageNo(-1), m_pdfEngine(nullptr) { }
    ~PdfFilter()  override { CleanUp(); }

    HRESULT OnInit() override;
    HRESULT GetNextChunkValue(ChunkValue &chunkValue) override;

    VOID CleanUp();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID *pClassID) {
        return CLSIDFromString(SZ_PDF_FILTER_HANDLER, pClassID);
    }

private:
    PDF_FILTER_STATE m_state;
    int m_iPageNo;
    EngineBase *m_pdfEngine;
};
