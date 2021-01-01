/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class HtmlPullParser;
struct HtmlToken;

char* NormalizeURL(const char* url, const char* base);

class PropertyMap {
    AutoFree values[(int)DocumentProperty::PdfVersion];

    int Find(DocumentProperty prop) const;

  public:
    void Set(DocumentProperty prop, char* valueUtf8, bool replace = false);
    WCHAR* Get(DocumentProperty prop) const;
};

/* ********** EPUB ********** */

class EpubDoc {
    MultiFormatArchive* zip = nullptr;
    // zip and images are the only mutable members of EpubDoc after initialization;
    // access to them must be serialized for multi-threaded users (such as EbookController)
    CRITICAL_SECTION zipAccess;

    str::Str htmlData;
    Vec<ImageData2> images;
    AutoFreeWstr tocPath;
    AutoFreeWstr fileName;
    PropertyMap props;
    bool isNcxToc = false;
    bool isRtlDoc = false;

    bool Load();
    void ParseMetadata(const char* content);
    bool ParseNavToc(const char* data, size_t dataLen, const char* pagePath, EbookTocVisitor* visitor);
    bool ParseNcxToc(const char* data, size_t dataLen, const char* pagePath, EbookTocVisitor* visitor);

  public:
    explicit EpubDoc(const WCHAR* fileName);
    explicit EpubDoc(IStream* stream);
    ~EpubDoc();

    std::span<u8> GetHtmlData() const;

    ImageData* GetImageData(const char* fileName, const char* pagePath);
    std::span<u8> GetFileData(const char* relPath, const char* pagePath);

    WCHAR* GetProperty(DocumentProperty prop) const;
    const WCHAR* GetFileName() const;
    bool IsRTL() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);

    static EpubDoc* CreateFromFile(const WCHAR* path);
    static EpubDoc* CreateFromStream(IStream* stream);
};

/* ********** FictionBook (FB2) ********** */

#define FB2_TOC_ENTRY_MARK "ToC!Entry!"

class Fb2Doc {
  public:
    AutoFreeWstr fileName;
    IStream* stream = nullptr;

    str::Str xmlData;
    Vec<ImageData2> images;
    AutoFree coverImage;
    PropertyMap props;
    bool isZipped = false;
    bool hasToc = false;

    bool Load();
    void ExtractImage(HtmlPullParser* parser, HtmlToken* tok);

    explicit Fb2Doc(const WCHAR* fileName);
    explicit Fb2Doc(IStream* stream);
    ~Fb2Doc();

    std::span<u8> GetXmlData() const;

    ImageData* GetImageData(const char* fileName);
    ImageData* GetCoverImage();

    WCHAR* GetProperty(DocumentProperty prop) const;
    const WCHAR* GetFileName() const;
    bool IsZipped() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);

    static Fb2Doc* CreateFromFile(const WCHAR* path);
    static Fb2Doc* CreateFromStream(IStream* stream);
};

/* ********** PalmDOC (and TealDoc) ********** */

class PdbReader;

class PalmDoc {
    AutoFreeWstr fileName;
    str::Str htmlData;
    WStrVec tocEntries;

    bool Load();

  public:
    explicit PalmDoc(const WCHAR* path);
    ~PalmDoc();

    std::span<u8> GetHtmlData() const;

    WCHAR* GetProperty(DocumentProperty prop) const;
    const WCHAR* GetFileName() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);
    static PalmDoc* CreateFromFile(const WCHAR* path);
};

/* ********** Plain HTML ********** */

class HtmlDoc {
    AutoFreeWstr fileName;
    AutoFree htmlData;
    AutoFree pagePath;
    Vec<ImageData2> images;
    PropertyMap props;

    bool Load();
    std::span<u8> LoadURL(const char* url);

  public:
    explicit HtmlDoc(const WCHAR* path);
    ~HtmlDoc();

    std::span<u8> GetHtmlData();

    ImageData* GetImageData(const char* fileName);
    std::span<u8> GetFileData(const char* relPath);

    WCHAR* GetProperty(DocumentProperty prop) const;
    const WCHAR* GetFileName() const;

    static bool IsSupportedFileType(Kind kind);
    static HtmlDoc* CreateFromFile(const WCHAR* fileName);
};

/* ********** Plain Text (and RFCs and TCR) ********** */

class TxtDoc {
    AutoFreeWstr fileName;
    str::Str htmlData;
    bool isRFC;

    bool Load();

  public:
    explicit TxtDoc(const WCHAR* fileName);

    std::span<u8> GetHtmlData() const;

    WCHAR* GetProperty(DocumentProperty prop) const;
    const WCHAR* GetFileName() const;

    bool IsRFC() const;
    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);
    static TxtDoc* CreateFromFile(const WCHAR* fileName);
};
