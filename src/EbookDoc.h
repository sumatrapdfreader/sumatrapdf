/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EpubDoc_h
#define EpubDoc_h

#include "EbookBase.h"
#include "ZipUtil.h"

struct ImageData2 {
    ImageData base;
    char *  id;  // path by which content refers to this image
    size_t  idx; // document specific index at which to find this image
};

char *NormalizeURL(const char *url, const char *base);

class PropertyMap {
    struct Data {
        DocumentProperty prop;
        char *value;
    };
    Vec<Data> props;

public:
    PropertyMap() { }
    ~PropertyMap();

    void Set(DocumentProperty prop, char *valueUtf8, bool replace=false);
    WCHAR *Get(DocumentProperty prop) const;
};

/* ********** EPUB ********** */

class EpubDoc {
    ZipFile zip;
    str::Str<char> htmlData;
    Vec<ImageData2> images;
    ScopedMem<WCHAR> tocPath;
    ScopedMem<WCHAR> fileName;
    PropertyMap props;
    bool isNcxToc;
    bool isRtlDoc;

    bool Load();
    void ParseMetadata(const char *content);
    bool ParseNavToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor);
    bool ParseNcxToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor);

public:
    EpubDoc(const WCHAR *fileName);
    EpubDoc(IStream *stream);
    ~EpubDoc();

    const char *GetTextData(size_t *lenOut);
    size_t GetTextDataSize();
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

class HtmlPullParser;
struct HtmlToken;

class Fb2Doc {
    ScopedMem<WCHAR> fileName;
    IStream *stream;

    str::Str<char> xmlData;
    Vec<ImageData2> images;
    ScopedMem<char> coverImage;
    PropertyMap props;

    bool isZipped;
    bool hasToc;

    bool Load();
    void ExtractImage(HtmlPullParser *parser, HtmlToken *tok);

public:
    Fb2Doc(const WCHAR *fileName);
    Fb2Doc(IStream *stream);
    ~Fb2Doc();

    const char *GetTextData(size_t *lenOut);
    size_t GetTextDataSize();
    ImageData *GetImageData(const char *id);
    ImageData *GetCoverImage();

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;
    bool IsZipped() const;

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static Fb2Doc *CreateFromFile(const WCHAR *fileName);
    static Fb2Doc *CreateFromStream(IStream *stream);
};

/* ********** PalmDOC (and TealDoc) ********** */

class PdbReader;

class PalmDoc {
    ScopedMem<WCHAR> fileName;
    str::Str<char> htmlData;
    Vec<ImageData2> images;
    WStrVec tocEntries;

    bool Load();

public:
    PalmDoc(const WCHAR *fileName);
    ~PalmDoc();

    const char *GetTextData(size_t *lenOut);

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static PalmDoc *CreateFromFile(const WCHAR *fileName);
};

/* ********** TCR (Text Compression for (Psion) Reader) ********** */

class TcrDoc {
    ScopedMem<WCHAR> fileName;
    str::Str<char> htmlData;

    bool Load();

public:
    TcrDoc(const WCHAR *fileName);
    ~TcrDoc();

    const char *GetTextData(size_t *lenOut);

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static TcrDoc *CreateFromFile(const WCHAR *fileName);
};

/* ********** Plain HTML ********** */

class HtmlDoc {
    ScopedMem<WCHAR> fileName;
    ScopedMem<char> htmlData;
    ScopedMem<char> pagePath;
    Vec<ImageData2> images;
    PropertyMap props;

    bool Load();
    char *LoadURL(const char *url, size_t *lenOut);

public:
    HtmlDoc(const WCHAR *fileName);
    ~HtmlDoc();

    const char *GetTextData(size_t *lenOut);
    ImageData *GetImageData(const char *id);
    char *GetFileData(const char *relPath, size_t *lenOut);

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static HtmlDoc *CreateFromFile(const WCHAR *fileName);
};

/* ********** Plain Text ********** */

class TxtDoc {
    ScopedMem<WCHAR> fileName;
    str::Str<char> htmlData;
    bool isRFC;

    bool Load();

public:
    TxtDoc(const WCHAR *fileName);

    const char *GetTextData(size_t *lenOut);

    WCHAR *GetProperty(DocumentProperty prop) const;
    const WCHAR *GetFileName() const;

    bool IsRFC() const;
    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static TxtDoc *CreateFromFile(const WCHAR *fileName);
};

#endif
