/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HuffDicDecompressor;
struct PdbReader;
struct PropValue;
enum class DocProp : u8;
enum class FileType : u8;

struct MobiDoc {
    Str fileName;

    PdbReader* pdbReader = nullptr;

    PdbDocType docType = PdbDocType::Unknown;
    int docRecCount = 0;
    int compressionType = 0;
    int docUncompressedSize = 0;
    int textEncoding = CP_UTF8;
    int docTocIndex = 0;

    bool multibyte = false;
    int trailersCount = 0;
    int imageFirstRec = 0; // 0 if no images
    int coverImageRec = 0; // 0 if no cover image

    Str* images = nullptr;

    HuffDicDecompressor* huffDic = nullptr;

    Vec<PropValue> props;

    explicit MobiDoc(Str filePath);

    bool ParseHeader();
    bool LoadDocRecordIntoBuffer(int recNo, str::Builder& strOut);
    void LoadImages();
    bool LoadImage(int imageNo);
    bool LoadForPdbReader(PdbReader* pdbReader);
    bool DecodeExthHeader(const u8* data, int dataLen);

    str::Builder doc;

    int imagesCount = 0;

    ~MobiDoc();

    Str GetHtmlData() const;
    Str GetCoverImage();
    Str GetImage(int imgRecIndex) const;
    Str GetFileName() const { return fileName; }
    TempStr GetPropertyTemp(DocProp prop);
    PdbDocType GetDocType() const { return docType; }

    bool HasToc();
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(FileType);
    static MobiDoc* CreateFromFile(Str fileName);
    static MobiDoc* CreateFromData(Str data);
};
