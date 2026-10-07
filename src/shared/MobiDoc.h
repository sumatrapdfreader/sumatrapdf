/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HuffDicDecompressor;
struct PdbReader;
struct PropValue;
enum class DocProp : u8;

struct MobiDoc {
    Str fileName;

    PdbReader* pdbReader = nullptr;

    PdbDocType docType = PdbDocType::Unknown;
    int docRecCount = 0;
    int compressionType = 0;
    int docUncompressedSize = 0;
    int textEncoding = CP_UTF8;
    int docTocIndex = -1;

    bool multibyte = false;
    int trailersCount = 0;
    int imageFirstRec = 0; // 0 if no images
    int coverImageRec = 0; // 0 if no cover image

    Vec<Str> images;

    HuffDicDecompressor* huffDic = nullptr;

    Vec<PropValue> props;

    explicit MobiDoc(Str filePath);

    bool ParseHeader();
    bool LoadDocRecordIntoBuffer(int recNo, str::Builder& strOut);
    void LoadImages();
    bool LoadForPdbReader(PdbReader* pdbReader);
    bool DecodeExthHeader(const u8* data, int dataLen);
    int CountLoadedImages() const;
    void MaybeSynthesizeImagePages();

    str::Builder doc;

    ~MobiDoc();

    Str GetHtmlData() const;
    Str GetCoverImage();
    Str GetImage(int imgRecIndex) const;
    TempStr GetPropertyTemp(DocProp prop);

    bool HasToc();
    bool ParseToc(EbookTocVisitor* visitor);

    static MobiDoc* CreateFromFile(Str path);
    static MobiDoc* CreateFromData(Str data);
};

int KindleEmbedToRecIndex(Str src);
