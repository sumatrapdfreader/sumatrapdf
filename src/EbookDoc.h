/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class HtmlPullParser;
struct HtmlToken;

struct ImageData2 {
    ImageData base;
    char *  id;  // path by which content refers to this image
    size_t  idx; // document specific index at which to find this image
};

char *NormalizeURL(const char *url, const char *base);

class PropertyMap {
    AutoFree values[Prop_PdfVersion];

    int Find(DocumentProperty prop) const;

public:
    void Set(DocumentProperty prop, char *valueUtf8, bool replace=false);
    WCHAR *Get(DocumentProperty prop) const;
};

/* ********** EPUB ********** */

class EpubDoc {
    ArchFile* zip = nullptr;
    // zip and images are the only mutable members of EpubDoc after initialization;
    // access to them must be serialized for multi-threaded users (such as EbookController)
    CRITICAL_SECTION zipAccess;

    str::Str<char> htmlData;
    Vec<ImageData2> images;
    AutoFreeW tocPath;
    AutoFreeW fileName;
    PropertyMap props;
    bool isNcxToc = false;
    bool isRtlDoc = false;

    bool Load();
    void ParseMetadata(const char *content);
    bool ParseNavToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor);
    bool ParseNcxToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor);

public:
    explicit EpubDoc(const WCHAR *fileName);
    explicit EpubDoc(IStream *stream);
    ~EpubDoc();

    const char *GetHtmlData(size_t *lenOut) const;
    size_t GetHtmlDataSize() const;
    ImageData *GetImageData(const char *id, const char *pagePath);
    char *GetFileData(const char *relPath, const char *pagePath, size_t *lenOut);

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;
    bool IsRTL() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static EpubDoc *CreateFromFile(const WCHAR *fileName);
    static EpubDoc *CreateFromStream(IStream *stream);
};

/* ********** FictionBook (FB2) ********** */

#define FB2_TOC_ENTRY_MARK "ToC!Entry!"

class Fb2Doc {
    AutoFreeW fileName;
    IStream *stream;

    str::Str<char> xmlData;
    Vec<ImageData2> images;
    AutoFree coverImage;
    PropertyMap props;
    bool isZipped;
    bool hasToc;

    bool Load();
    void ExtractImage(HtmlPullParser *parser, HtmlToken *tok);

public:
    explicit Fb2Doc(const WCHAR *fileName);
    explicit Fb2Doc(IStream *stream);
    ~Fb2Doc();

    const char *GetXmlData(size_t *lenOut) const;
    size_t GetXmlDataSize() const;
    ImageData *GetImageData(const char *id);
    ImageData *GetCoverImage();

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;
    bool IsZipped() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static Fb2Doc *CreateFromFile(const WCHAR *fileName);
    static Fb2Doc *CreateFromStream(IStream *stream);
};

/* ********** PalmDOC (and TealDoc) ********** */

class PdbReader;

class PalmDoc {
    AutoFreeW fileName;
    str::Str<char> htmlData;
    WStrVec tocEntries;

    bool Load();

public:
    explicit PalmDoc(const WCHAR *fileName);
    ~PalmDoc();

    const char *GetHtmlData(size_t *lenOut) const;
    size_t GetHtmlDataSize() const;

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static PalmDoc *CreateFromFile(const WCHAR *fileName);
};

/* ********** Plain HTML ********** */

class HtmlDoc {
    AutoFreeW fileName;
    AutoFree htmlData;
    AutoFree pagePath;
    Vec<ImageData2> images;
    PropertyMap props;

    bool Load();
    char *LoadURL(const char *url, size_t *lenOut);

public:
    explicit HtmlDoc(const WCHAR *fileName);
    ~HtmlDoc();

    const char *GetHtmlData(size_t *lenOut) const;
    ImageData *GetImageData(const char *id);
    char *GetFileData(const char *relPath, size_t *lenOut);

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static HtmlDoc *CreateFromFile(const WCHAR *fileName);
};

/* ********** Plain Text (and RFCs and TCR) ********** */

class TxtDoc {
    AutoFreeW fileName;
    str::Str<char> htmlData;
    bool isRFC;

    bool Load();

public:
    explicit TxtDoc(const WCHAR *fileName);

    const char *GetHtmlData(size_t *lenOut) const;

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;

    bool IsRFC() const;
    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static TxtDoc *CreateFromFile(const WCHAR *fileName);
};
