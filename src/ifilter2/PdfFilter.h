/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr const char* kPdfFilterStateStrs = "start\0author\0title\0data\0content\0end\0";
enum class PdfFilterState {
    Start = 0,
    Author,
    Title,
    Date,
    Content,
    End
};

// Data extracted via pipe communication with SumatraPDF.exe
struct IFilterExtractedData {
    char* author = nullptr;
    char* title = nullptr;
    char* date = nullptr;
    int pageCount = 0;
    WCHAR** pageTexts = nullptr; // array of page texts (pageCount elements)
};

void FreeExtractedData(IFilterExtractedData* data);

// File type enum - must match SumatraStartup.cpp
enum class IFilterFileType : u32 {
    PDF = 1,
    EPUB = 2,
};

// Extract data via pipe communication with SumatraPDF.exe
IFilterExtractedData* ExtractDataViaPipe(IFilterFileType fileType, const ByteSlice& fileData);

class PdfFilter : public FilterBase {
  public:
    PdfFilter(long* plRefCount) : FilterBase(plRefCount) {
    }
    ~PdfFilter() override {
        CleanUp();
    }

    HRESULT OnInit() override;
    HRESULT GetNextChunkValue(ChunkValue& chunkValue) override;

    VOID CleanUp();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID* pClassID) {
        return CLSIDFromString(kPdfFilterHandler, pClassID);
    }

  private:
    PdfFilterState m_state{PdfFilterState::End};
    int m_iPageNo = -1;
    IFilterExtractedData* m_extractedData = nullptr;
};
