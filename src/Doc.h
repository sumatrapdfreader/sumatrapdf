/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Doc_h
#define Doc_h

// In the past operations on supported document files were done as BaseEngine
// subclass. When we added MobiDoc and EbookWindow, not every document is an engine.
// Class Doc (a short for Document, since it's going to be used frequently)
// is a wrapper/abstractions for them.
// It simply wraps all document objects, allows querying the type, casting
// to the wrapped object and present as much of the unified interface as
// possible.
// It's small enough to be passed as value.
// 
// During transitional period, there wil be a lot of converting to/from
// but the intent is to use Doc and only extract the wrapped object at
// leaf nodes of the code

class BaseEngine;
class CbxEngine;
class ChmEngine;
class Chm2Engine;
class DjVuEngine;
class EpubDoc;
class EpubEngine;
class Fb2Engine;
class ImageEngine;
class ImageDirEngine;
class MobiEngine;
class MobiDoc;
class MobiTestDoc;
class PdfEngine;
class PsEngine;
class XpsEngine;

struct ImageData;

class Doc
{
public:
    enum DocType {
        None,
        CbxEng, ChmEng, Chm2Eng, DjVuEng, Epub,
        EpubEng, Fb2Eng, ImageEng, ImageDirEng,
        MobiEng, Mobi, MobiTest, PdfEng, PsEng, XpsEng
    };

    DocType type;

    // If there was an error loading a file in CreateFromFile(),
    // this is an error message to be shown to the user. Most
    // of the time it's a generic error message but can be
    // more specific e.g. mobi loading might specify that
    // loading failed due to DRM
    // The caller can also display it's own message.
    //
    // If it's set, it also marks a specific state: we tried to
    // load the file but failed (wchich can be useful e.g.
    // for implementing reloading)
    TCHAR *loadingErrorMessage;

    // A copy of the file path. We need this because while in
    // most cases we can extract it from the wrapped object,
    // if loading failed we don't have it but might need it
    TCHAR *filePath;

    union {
        void *generic;
        BaseEngine *engine; // we can always cast to the right type based on type
        EpubDoc * epubDoc;
        MobiDoc * mobiDoc;
        MobiTestDoc *mobiTestDoc;
    };

    Doc(const Doc& other);
    Doc& operator=(const Doc& other);
    ~Doc();

    void Clear() { type = None; loadingErrorMessage = NULL; filePath = NULL; generic = NULL; }
    Doc() { Clear(); }
    Doc(CbxEngine *doc) { Set(doc); }
    Doc(ChmEngine *doc) { Set(doc); }
    Doc(Chm2Engine *doc) { Set(doc); }
    Doc(DjVuEngine *doc) { Set(doc); }
    Doc(EpubDoc *doc) { Set(doc); }
    Doc(EpubEngine *doc) { Set(doc); }
    Doc(Fb2Engine *doc) { Set(doc); }
    Doc(ImageEngine *doc) { Set(doc); }
    Doc(ImageDirEngine *doc)  { Set(doc); }
    Doc(MobiEngine *doc) { Set(doc); }
    Doc(MobiDoc *doc) { Set(doc); }
    Doc(MobiTestDoc *doc) { Set(doc); }
    Doc(PdfEngine *doc) { Set(doc); }
    Doc(PsEngine *doc) { Set(doc); }
    Doc(XpsEngine *doc) { Set(doc); }

    void Delete();

    // note: find a better name, if possible
    bool IsNone() const { return None == type; }
    bool IsEbook() const;
    bool IsEngine() const { return !IsNone() && !IsEbook(); }

    bool LoadingFailed() const { return NULL != loadingErrorMessage; }

    BaseEngine *AsEngine() const;
    MobiDoc *AsMobi() const;
    MobiTestDoc *AsMobiTest() const;
    EpubDoc *AsEpub() const;

    // instead of adding these to Doc, they could also be part
    // of a virtual EbookDoc interface that *Doc implement so
    // that the compiler can choose the correct method automatically
    const TCHAR *GetFilePath() const;
    TCHAR *GetProperty(const char *name);
    const char *GetHtmlData(size_t &len);
    size_t GetHtmlDataSize();
    ImageData *GetCoverImage();

    static Doc CreateFromFile(const TCHAR *filePath);

private:
    // Set() only called from the constructor and it really sets (and not replaces)
    void Set(CbxEngine *doc);
    void Set(ChmEngine *doc);
    void Set(Chm2Engine *doc);
    void Set(DjVuEngine *doc);
    void Set(EpubEngine *doc);
    void Set(EpubDoc *doc);
    void Set(Fb2Engine *doc);
    void Set(ImageEngine *doc);
    void Set(ImageDirEngine *doc);
    void Set(MobiEngine *doc);
    void Set(MobiDoc *doc);
    void Set(MobiTestDoc *doc);
    void Set(PdfEngine *doc);
    void Set(PsEngine *doc);
    void Set(XpsEngine *doc);

    const TCHAR *GetFilePathFromDoc() const;
    void FreeStrings();
    void SetEngine(DocType newType, BaseEngine *doc);
};

#endif
