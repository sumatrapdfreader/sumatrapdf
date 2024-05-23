/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class HtmlPullParser;
struct HtmlToken;

struct ImageData {
    ByteSlice base;
    // path by which content refers to this image
    char* fileName = nullptr;
    // document specific id by whcih to find this image
    size_t fileId{0};
};

char* NormalizeURL(const char* url, const char* base);

struct PropertyMap {
    char* values[(int)DocumentProperty::PdfVersion] = {0};

    ~PropertyMap();
    int Find(DocumentProperty prop) const;

  public:
    void SetVal(DocumentProperty prop, char* val, bool setIfExists = false);
    TempStr GetTemp(DocumentProperty prop) const;
};

/* ********** EPUB ********** */

class EpubDoc {
    MultiFormatArchive* zip = nullptr;
    // zip and images are the only mutable members of EpubDoc after initialization;
    // access to them must be serialized for multi-threaded users
    CRITICAL_SECTION zipAccess;

    str::Str htmlData;
    Vec<ImageData> images;
    AutoFreeStr tocPath;
    AutoFreeStr fileName;
    PropertyMap props;
    bool isNcxToc = false;
    bool isRtlDoc = false;

    bool Load();
    void ParseMetadata(const char* content);
    bool ParseNavToc(const char* data, size_t dataLen, const char* pagePath, EbookTocVisitor* visitor);
    bool ParseNcxToc(const char* data, size_t dataLen, const char* pagePath, EbookTocVisitor* visitor);

  public:
    explicit EpubDoc(const char* fileName);
    explicit EpubDoc(IStream* stream);
    ~EpubDoc();

    ByteSlice GetHtmlData() const;

    ByteSlice* GetImageData(const char* fileName, const char* pagePath);
    ByteSlice GetFileData(const char* relPath, const char* pagePath);

    TempStr GetPropertyTemp(DocumentProperty prop) const;
    const char* GetFileName() const;
    bool IsRTL() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);

    static EpubDoc* CreateFromFile(const char* path);
    static EpubDoc* CreateFromStream(IStream* stream);
};

/* ********** FictionBook (FB2) ********** */

#define FB2_TOC_ENTRY_MARK "ToC!Entry!"

class Fb2Doc {
  public:
    AutoFreeStr fileName;
    IStream* stream = nullptr;

    str::Str xmlData;
    Vec<ImageData> images;
    AutoFree coverImage;
    PropertyMap props;
    bool isZipped = false;
    bool hasToc = false;

    bool Load();
    void ExtractImage(HtmlPullParser* parser, HtmlToken* tok);

    explicit Fb2Doc(const char* fileName);
    explicit Fb2Doc(IStream* stream);
    ~Fb2Doc();

    ByteSlice GetXmlData() const;

    ByteSlice* GetImageData(const char* fileName) const;
    ByteSlice* GetCoverImage() const;

    TempStr GetPropertyTemp(DocumentProperty prop) const;
    const char* GetFileName() const;
    bool IsZipped() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor) const;

    static bool IsSupportedFileType(Kind kind);

    static Fb2Doc* CreateFromFile(const char* path);
    static Fb2Doc* CreateFromStream(IStream* stream);
};

/* ********** PalmDOC (and TealDoc) ********** */

class PdbReader;

class PalmDoc {
    AutoFreeStr fileName;
    str::Str htmlData;
    StrVec tocEntries;

    bool Load();

  public:
    explicit PalmDoc(const char* path);
    ~PalmDoc();

    ByteSlice GetHtmlData() const;

    TempStr GetPropertyTemp(DocumentProperty prop) const;
    const char* GetFileName() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);
    static PalmDoc* CreateFromFile(const char* path);
};

/* ********** Plain HTML ********** */

class HtmlDoc {
    AutoFreeStr fileName;
    ByteSlice htmlData;
    AutoFreeStr pagePath;
    Vec<ImageData> images;
    PropertyMap props;

    bool Load();
    ByteSlice LoadURL(const char* url);

  public:
    explicit HtmlDoc(const char* path);
    ~HtmlDoc();

    ByteSlice GetHtmlData();

    ByteSlice* GetImageData(const char* fileName);
    ByteSlice GetFileData(const char* relPath);

    TempStr GetPropertyTemp(DocumentProperty prop) const;
    const char* GetFileName() const;

    static bool IsSupportedFileType(Kind kind);
    static HtmlDoc* CreateFromFile(const char* fileName);
};

/* ********** Plain Text (and RFCs and TCR) ********** */

class TxtDoc {
    AutoFreeStr fileName;
    str::Str htmlData;
    bool isRFC = false;

    bool Load();

  public:
    explicit TxtDoc(const char* fileName);

    ByteSlice GetHtmlData() const;

    TempStr GetPropertyTemp(DocumentProperty prop) const;
    const char* GetFileName() const;

    bool IsRFC() const;
    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);
    static TxtDoc* CreateFromFile(const char* fileName);
};
