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

class EpubDoc {
    ZipFile zip;
    str::Str<char> htmlData;
    Vec<ImageData2> images;
    Vec<const char *> props;
    ScopedMem<TCHAR> tocPath;
    ScopedMem<TCHAR> fileName;

    bool Load();
    void ParseMetadata(const char *content);

    static bool VerifyEpub(ZipFile& zip);

public:
    EpubDoc(const TCHAR *fileName);
    EpubDoc(IStream *stream);
    ~EpubDoc();

    const char *GetTextData(size_t *lenOut);
    size_t GetTextDataSize();
    ImageData *GetImageData(const char *id, const char *pagePath);

    TCHAR *GetProperty(const char *name);
    const TCHAR *GetFileName() const;

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static EpubDoc *CreateFromFile(const TCHAR *fileName);
    static EpubDoc *CreateFromStream(IStream *stream);
};

class HtmlPullParser;
struct HtmlToken;

class Fb2Doc {
    ScopedMem<TCHAR> fileName;
    str::Str<char> xmlData;
    Vec<ImageData2> images;
    ScopedMem<TCHAR> docTitle;
    ScopedMem<TCHAR> docAuthor;
    ScopedMem<char> hrefName;

    bool isZipped;
    bool hasToc;

    bool Load();
    void ExtractImage(HtmlPullParser *parser, HtmlToken *tok);

public:
    Fb2Doc(const TCHAR *fileName);
    ~Fb2Doc();

    const char *GetTextData(size_t *lenOut);
    ImageData *GetImageData(const char *id);

    TCHAR *GetProperty(const char *name);
    const TCHAR *GetFileName() const;
    const char *GetHrefName() const;
    bool IsZipped() const;

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static Fb2Doc *CreateFromFile(const TCHAR *fileName);
};

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

class HtmlDoc {
    ScopedMem<TCHAR> fileName;
    ScopedMem<char> htmlData;
    ScopedMem<char> pagePath;
    Vec<ImageData2> images;

    bool Load();

public:
    HtmlDoc(const TCHAR *fileName);
    ~HtmlDoc();

    const char *GetTextData(size_t *lenOut);
    ImageData *GetImageData(const char *id);

    const TCHAR *GetFileName() const;

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static HtmlDoc *CreateFromFile(const TCHAR *fileName);
};

char *NormalizeURL(const char *url, const char *base);

#endif
