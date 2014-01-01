/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
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
class EpubDoc;
class Fb2Doc;
class MobiDoc;
class MobiTestDoc;
class PasswordUI;

struct ImageData;
enum DocumentProperty;

enum DocType {
    Doc_None = 0, Engine_None = 0,

    Doc_Epub, Doc_Fb2,
    Doc_Mobi, Doc_MobiTest,

    // the EngineManager tries to create a new engine
    // in the following order (types on the same line
    // share common code and reside in the same file)
    Engine_PDF, Engine_XPS,
    Engine_DjVu,
    Engine_Image, Engine_ImageDir, Engine_ComicBook,
    Engine_PS,
    Engine_Chm,
    Engine_Epub, Engine_Fb2, Engine_Mobi, Engine_Pdb, Engine_Chm2, Engine_Tcr, Engine_Html, Engine_Txt,
};

enum DocError {
    Error_None, Error_Unknown,
};

class Doc
{
protected:
    DocType type;

    // If there was an error loading a file in CreateFromFile(),
    // this is an error message to be shown to the user. Most
    // of the time it's a generic error message but can be
    // more specific e.g. mobi loading might specify that
    // loading failed due to DRM
    DocError error;

    // A copy of the file path which is needed in case of an error (else
    // the file path is supposed to be stored inside the wrapped *Doc or engine)
    ScopedMem<WCHAR> filePath;

    union {
        void *generic;
        BaseEngine *engine; // we can always cast to the right type based on type
        EpubDoc *   epubDoc;
        Fb2Doc *    fb2Doc;
        MobiDoc *   mobiDoc;
        MobiTestDoc *mobiTestDoc;
    };

    const WCHAR *GetFilePathFromDoc() const;

public:
    Doc(const Doc& other);
    Doc& operator=(const Doc& other);
    ~Doc();

    void Clear();
    Doc() { Clear(); }
    Doc(BaseEngine *doc, DocType engineType);
    Doc(EpubDoc *doc);
    Doc(Fb2Doc *doc);
    Doc(MobiDoc *doc);
    Doc(MobiTestDoc *doc);

    void Delete();

    // note: find a better name, if possible
    bool IsNone() const { return Doc_None == type; }
    // to allow distinguishing loading errors from blank docs
    bool IsEbook() const;
    bool IsEngine() const;

    bool LoadingFailed() const {
        CrashIf(error && !IsNone());
        return error != Error_None;
    }

    BaseEngine *AsEngine() const;
    EpubDoc *AsEpub() const;
    Fb2Doc *AsFb2() const;
    MobiDoc *AsMobi() const;
    MobiTestDoc *AsMobiTest() const;

    DocType GetDocType() const { return type; }

    // instead of adding these to Doc, they could also be part
    // of a virtual EbookDoc interface that *Doc implement so
    // that the compiler can choose the correct method automatically
    const WCHAR *GetFilePath() const;
    WCHAR *GetProperty(DocumentProperty prop);
    const char *GetHtmlData(size_t &len);
    size_t GetHtmlDataSize();
    ImageData *GetCoverImage();

    static Doc CreateFromFile(const WCHAR *filePath);
};

namespace EngineManager {

bool IsSupportedFile(const WCHAR *filePath, bool sniff=false, bool enableEbookEngines=true);
BaseEngine *CreateEngine(const WCHAR *filePath, PasswordUI *pwdUI=NULL, DocType *typeOut=NULL, bool useAlternateChmEngine=false, bool enableEbookEngines=true);

inline BaseEngine *CreateEngine(const WCHAR *filePath, bool useAlternateChmEngine) {
    return CreateEngine(filePath, NULL, NULL, useAlternateChmEngine, true);
}

}

#endif
