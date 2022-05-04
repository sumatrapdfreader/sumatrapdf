/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr const char* kPdfFilterStateStrs = "start\0author\0title\0data\0content\0end\0";
enum class PdfFilterState { Start = 0, Author, Title, Date, Content, End };

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
        return CLSIDFromString(kPdfFilterHandler, pClassID);
    }

private:
    PdfFilterState m_state{PdfFilterState::End};
    int m_iPageNo = -1;
    EngineBase *m_pdfEngine = nullptr;
};
