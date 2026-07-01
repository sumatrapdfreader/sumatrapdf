/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HuffDicDecompressor;
struct PdbReader;

struct MobiDoc {
    Str fileName;

    PdbReader* pdbReader = nullptr;

    PdbDocType docType = PdbDocType::Unknown;
    size_t docRecCount = 0;
    int compressionType = 0;
    size_t docUncompressedSize = 0;
    int textEncoding = CP_UTF8;
    size_t docTocIndex = 0;

    bool multibyte = false;
    size_t trailersCount = 0;
    size_t imageFirstRec = 0; // 0 if no images
    size_t coverImageRec = 0; // 0 if no cover image

    ByteSlice* images = nullptr;

    HuffDicDecompressor* huffDic = nullptr;

    Props props;

    explicit MobiDoc(Str filePath);

    bool ParseHeader();
    bool LoadDocRecordIntoBuffer(size_t recNo, str::Builder& strOut);
    void LoadImages();
    bool LoadImage(size_t imageNo);
    bool LoadForPdbReader(PdbReader* pdbReader);
    bool DecodeExthHeader(const u8* data, size_t dataLen);

    str::Builder* doc = nullptr;

    size_t imagesCount = 0;

    ~MobiDoc();

    Str GetHtmlData() const;
    ByteSlice* GetCoverImage();
    ByteSlice* GetImage(size_t imgRecIndex) const;
    Str GetFileName() const { return fileName; }
    TempStr GetPropertyTemp(Str name);
    PdbDocType GetDocType() const { return docType; }

    bool HasToc();
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind);
    static MobiDoc* CreateFromFile(Str fileName);
    static MobiDoc* CreateFromStream(IStream* stream);
};
