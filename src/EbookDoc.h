/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class GumboHtmlParser;
struct HtmlToken;

struct ImageData {
    Str base;
    // path by which content refers to this image
    Str fileName;
    // document specific id by whcih to find this image
    int fileId{0};
};

TempStr NormalizeURLTemp(Str url, Str base);

/* ********** EPUB ********** */

struct EpubDoc {
    MultiFormatArchive* archive = nullptr;
    // zip and images are the only mutable members of EpubDoc after initialization;
    // access to them must be serialized for multi-threaded users
    CRITICAL_SECTION zipAccess;

    str::Builder htmlData;
    Vec<ImageData> images;
    Str tocPath;
    Str fileName;
    Props props;
    bool isNcxToc = false;
    bool isRtlDoc = false;

    bool Load();

    explicit EpubDoc(Str fileName);
    explicit EpubDoc(IStream* stream);
    ~EpubDoc();

    Str GetHtmlData() const;

    Str GetImageData(Str fileName, Str pagePath);
    Str GetFileData(Str relPath, Str pagePath);

    TempStr GetPropertyTemp(Str name) const;
    Str GetFileName() const;
    bool IsRTL() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);

    static EpubDoc* CreateFromFile(Str path);
    static EpubDoc* CreateFromStream(IStream* stream);
};

/* ********** FictionBook (FB2) ********** */

#define FB2_TOC_ENTRY_MARK "ToC!Entry!"

struct Fb2Doc {
    Str fileName;
    IStream* stream = nullptr;

    str::Builder xmlData;
    Vec<ImageData> images;
    Str coverImage;
    Props props;
    bool isZipped = false;
    bool hasToc = false;

    bool Load();
    void ExtractImage(GumboHtmlParser* parser, HtmlToken* tok);

    explicit Fb2Doc(Str fileName);
    explicit Fb2Doc(IStream* stream);
    ~Fb2Doc();

    Str GetXmlData() const;

    Str GetImageData(Str fileName) const;
    Str GetCoverImage() const;

    TempStr GetPropertyTemp(Str name) const;
    Str GetFileName() const;
    bool IsZipped() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor) const;

    static bool IsSupportedFileType(Kind kind);

    static Fb2Doc* CreateFromFile(Str path);
    static Fb2Doc* CreateFromStream(IStream* stream);
};

/* ********** PalmDOC (and TealDoc) ********** */

struct PdbReader;

struct PalmDoc {
    Str fileName;
    str::Builder htmlData;
    StrVec tocEntries;

    bool Load();

    explicit PalmDoc(Str path);
    ~PalmDoc();

    Str GetHtmlData() const;

    TempStr GetPropertyTemp(Str name) const;
    Str GetFileName() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);
    static PalmDoc* CreateFromFile(Str path);
};

/* ********** Plain HTML ********** */

struct HtmlDoc {
    Str fileName;
    Str htmlData;
    Str pagePath;
    Vec<ImageData> images;
    Props props;

    bool Load();
    Str LoadURL(Str url);

    explicit HtmlDoc(Str path);
    ~HtmlDoc();

    Str GetHtmlData();

    Str GetImageData(Str fileName);
    Str GetFileData(Str relPath);

    TempStr GetPropertyTemp(Str name) const;
    Str GetFileName() const;

    static bool IsSupportedFileType(Kind kind);
    static HtmlDoc* CreateFromFile(Str fileName);
};

/* ********** Plain Text (and RFCs and TCR) ********** */

struct TxtDoc {
    Str fileName;
    str::Builder htmlData;
    bool isRFC = false;

    bool Load();

    explicit TxtDoc(Str fileName);
    ~TxtDoc();

    Str GetHtmlData() const;

    TempStr GetPropertyTemp(Str name) const;
    Str GetFileName() const;

    bool IsRFC() const;
    bool HasToc() const;
    bool ParseToc(EbookTocVisitor* visitor);

    static bool IsSupportedFileType(Kind kind);
    static TxtDoc* CreateFromFile(Str fileName);
};
