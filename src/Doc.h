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
class EpubDoc;
class MobiDoc;
class MobiTestDoc;
class PasswordUI;

struct ImageData;

enum EngineType {
    Engine_None,
    Engine_DjVu,
    Engine_Image, Engine_ImageDir, Engine_ComicBook,
    Engine_PDF, Engine_XPS,
    Engine_PS,
    Engine_Chm,
    Engine_Epub, Engine_Fb2, Engine_Mobi, Engine_Pdb, Engine_Chm2, Engine_Html, Engine_Txt,
};

class Doc
{
public:
    enum DocType { None, Engine, Epub, Error, Mobi, MobiTest };

protected:
    DocType type;
    // reuse the enumeration instead of trying
    // to keep DocType in sync with EngineType
    EngineType engineType;

    // If there was an error loading a file in CreateFromFile(),
    // this is an error message to be shown to the user. Most
    // of the time it's a generic error message but can be
    // more specific e.g. mobi loading might specify that
    // loading failed due to DRM
    // The caller can also display it's own message.
    //
    // TODO: replace with an enumeration so that translation
    //       can more easily happen in UI code?
    TCHAR *loadingErrorMessage;

    // A copy of the file path. We need this because while in
    // most cases we can extract it from the wrapped object,
    // if loading failed we don't have it but might need it
    TCHAR *filePath;

    union {
        void *generic;
        BaseEngine *engine; // we can always cast to the right type based on engineType
        EpubDoc * epubDoc;
        MobiDoc * mobiDoc;
        MobiTestDoc *mobiTestDoc;
    };

    const TCHAR *GetFilePathFromDoc() const;
    void FreeStrings();

public:
    Doc(const Doc& other);
    Doc& operator=(const Doc& other);
    ~Doc();

    void Clear();
    Doc() { Clear(); }
    Doc(BaseEngine *doc, EngineType engineType);
    Doc(EpubDoc *doc);
    Doc(MobiDoc *doc);
    Doc(MobiTestDoc *doc);

    void Delete();

    // note: find a better name, if possible
    bool IsNone() const { return None == type; }
    // to allow distinguishing loading errors from blank docs
    bool IsError() const { return Error == type; }
    bool IsEbook() const;
    bool IsEngine() const { return Engine == type; }

    bool LoadingFailed() const {
        CrashIf(loadingErrorMessage && !IsError());
        return NULL != loadingErrorMessage;
    }

    BaseEngine *AsEngine() const;
    EpubDoc *AsEpub() const;
    MobiDoc *AsMobi() const;
    MobiTestDoc *AsMobiTest() const;

    EngineType GetEngineType() const { return engineType; }

    // instead of adding these to Doc, they could also be part
    // of a virtual EbookDoc interface that *Doc implement so
    // that the compiler can choose the correct method automatically
    const TCHAR *GetFilePath() const;
    TCHAR *GetProperty(const char *name);
    const char *GetHtmlData(size_t &len);
    size_t GetHtmlDataSize();
    ImageData *GetCoverImage();

    static Doc CreateFromFile(const TCHAR *filePath);
};

class EngineManager {
    bool enableEbookEngines;

public:
    EngineManager(bool enableEbookEngines=false) : enableEbookEngines(enableEbookEngines) { }
    bool IsSupportedFile(const TCHAR *filePath, bool sniff=false);
    BaseEngine *CreateEngine(const TCHAR *filePath, PasswordUI *pwdUI=NULL, EngineType *typeOut=NULL);
};

#endif
