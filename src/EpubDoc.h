/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
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
void UrlDecode(char *url);
void UrlDecode(WCHAR *url);

/* ********** EPUB ********** */

class EpubDoc {
    struct Metadata {
        DocumentProperty prop;
        char *value;
    };

    ZipFile zip;
    str::Str<char> htmlData;
    Vec<ImageData2> images;
    Vec<Metadata> props;
    ScopedMem<TCHAR> tocPath;
    ScopedMem<TCHAR> fileName;
    bool isNcxToc;
    bool isRtlDoc;

    bool Load();
    void ParseMetadata(const char *content);
    bool ParseNavToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor);
    bool ParseNcxToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor);

public:
    EpubDoc(const TCHAR *fileName);
    EpubDoc(IStream *stream);
    ~EpubDoc();

    const char *GetTextData(size_t *lenOut);
    size_t GetTextDataSize();
    ImageData *GetImageData(const char *id, const char *pagePath);

    TCHAR *GetProperty(DocumentProperty prop);
    const TCHAR *GetFileName() const;
    bool IsRTL() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static EpubDoc *CreateFromFile(const TCHAR *fileName);
    static EpubDoc *CreateFromStream(IStream *stream);
};

/* ********** FictionBook (FB2) ********** */

class HtmlPullParser;
struct HtmlToken;

class Fb2Doc {
    ScopedMem<TCHAR> fileName;
    str::Str<char> xmlData;
    Vec<ImageData2> images;
    ScopedMem<TCHAR> docTitle;
    ScopedMem<TCHAR> docAuthor;
    ScopedMem<char> coverImage;

    bool isZipped;
    bool hasToc;

    bool Load();
    void ExtractImage(HtmlPullParser *parser, HtmlToken *tok);

public:
    Fb2Doc(const TCHAR *fileName);
    ~Fb2Doc();

    const char *GetTextData(size_t *lenOut);
    ImageData *GetImageData(const char *id);
    ImageData *GetCoverImage();

    TCHAR *GetProperty(DocumentProperty prop);
    const TCHAR *GetFileName() const;
    bool IsZipped() const;

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static Fb2Doc *CreateFromFile(const TCHAR *fileName);
};

/* ********** PalmDOC (and TealDoc) ********** */

class PdbReader;

class PalmDoc {
    ScopedMem<TCHAR> fileName;
    str::Str<char> htmlData;
    Vec<ImageData2> images;
    StrVec tocEntries;

    bool Load();
    char *LoadTealPaintImage(const TCHAR *dbFile, size_t idx, size_t *lenOut);
    bool LoadTealPaintImageTile(PdbReader *pdbReader, size_t idx, uint8_t *pixels, int left, int top, int width, int height, int stride, bool hasPalette);
    char *GetTealPaintImageName(PdbReader *pdbReader, size_t idx, bool& isValid);

public:
    PalmDoc(const TCHAR *fileName);
    ~PalmDoc();

    const char *GetTextData(size_t *lenOut);
    ImageData *GetImageData(const char *id);
    const TCHAR *GetFileName() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static PalmDoc *CreateFromFile(const TCHAR *fileName);
};

/* ********** TCR (Text Compression for (Psion) Reader) ********** */

class TcrDoc {
    ScopedMem<TCHAR> fileName;
    str::Str<char> htmlData;

    bool Load();

public:
    TcrDoc(const TCHAR *fileName);
    ~TcrDoc();

    const char *GetTextData(size_t *lenOut);
    const TCHAR *GetFileName() const;

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static TcrDoc *CreateFromFile(const TCHAR *fileName);
};

/* ********** Plain HTML ********** */

class HtmlDoc {
    ScopedMem<TCHAR> fileName;
    ScopedMem<char> htmlData;
    ScopedMem<char> pagePath;
    Vec<ImageData2> images;

    ScopedMem<TCHAR> title;
    ScopedMem<TCHAR> author;
    ScopedMem<TCHAR> date;
    ScopedMem<TCHAR> copyright;

    bool Load();

public:
    HtmlDoc(const TCHAR *fileName);
    ~HtmlDoc();

    const char *GetTextData(size_t *lenOut);
    ImageData *GetImageData(const char *id);

    TCHAR *GetProperty(DocumentProperty prop);
    const TCHAR *GetFileName() const;

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static HtmlDoc *CreateFromFile(const TCHAR *fileName);
};

/* ********** Plain Text ********** */

class TxtDoc {
    ScopedMem<TCHAR> fileName;
    str::Str<char> htmlData;
    bool isRFC;

    bool Load();

public:
    TxtDoc(const TCHAR *fileName);

    const char *GetTextData(size_t *lenOut);
    const TCHAR *GetFileName() const;

    bool IsRFC() const;
    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static TxtDoc *CreateFromFile(const TCHAR *fileName);
};

#endif
