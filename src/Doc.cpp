/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Doc.h"

#include "BaseEngine.h"
#include "ChmEngine.h"
#include "DjVuEngine.h"
#include "EpubEngine.h"
#include "ImagesEngine.h"
#include "PdfEngine.h"
#include "PsEngine.h"

#include "EpubDoc.h"
#include "MobiDoc.h"

Doc::Doc(const Doc& other)
{
    Clear();
    type = other.type;
    generic = other.generic;
    str::ReplacePtr(&loadingErrorMessage, other.loadingErrorMessage);
    str::ReplacePtr(&filePath, other.filePath);
}

Doc& Doc::operator=(const Doc& other)
{
    if (this != &other) {
        type = other.type;
        generic = other.generic;
        str::ReplacePtr(&loadingErrorMessage, other.loadingErrorMessage);
        str::ReplacePtr(&filePath, other.filePath);
    }
    return *this;
}

Doc::~Doc()
{
    FreeStrings();
}

void Doc::FreeStrings()
{
    str::ReplacePtr(&loadingErrorMessage, NULL);
    str::ReplacePtr(&filePath, NULL);
}

// delete underlying object
void Doc::Delete()
{
    switch (type) {
    case Engine:
        delete engine;
        break;
    case Epub:
        delete epubDoc;
        break;
    case Error:
        break;
    case Mobi:
        delete mobiDoc;
        break;
    case MobiTest:
        delete mobiTestDoc;
        break;
    case None:
        break;
    default:
        CrashIf(true);
        break;
    }

    FreeStrings();
    Clear();
}

Doc::Doc(BaseEngine *doc, EngineType newEngineType)
{
    Clear();
    type = doc ? Engine : Error;
    engine = doc;
    engineType = newEngineType;
    CrashIf(Engine_None == engineType);
}

Doc::Doc(EpubDoc *doc)
{
    Clear();
    type = doc ? Epub : Error;
    epubDoc = doc;
}

Doc::Doc(MobiDoc *doc)
{
    Clear();
    type = doc ? Mobi : Error;
    mobiDoc = doc;
}

Doc::Doc(MobiTestDoc *doc)
{ 
    Clear();
    type = doc ? MobiTest : Error;
    mobiTestDoc = doc;
}

void Doc::Clear()
{
    type = None;
    engineType = Engine_None;
    loadingErrorMessage = NULL;
    filePath = NULL;
    generic = NULL;
}

BaseEngine *Doc::AsEngine() const
{
    if (Engine == type)
        return engine;
    return NULL;
}

MobiDoc *Doc::AsMobi() const
{
    if (Mobi == type)
        return mobiDoc;
    return NULL;
}

MobiTestDoc *Doc::AsMobiTest() const
{
    if (MobiTest == type)
        return mobiTestDoc;
    return NULL;
}

EpubDoc *Doc::AsEpub() const
{
    if (Epub == type)
        return epubDoc;
    return NULL;
}

// return true if this is document supported by ebook UI
bool Doc::IsEbook() const
{
    return (Mobi == type) || (MobiTest == type) || (Epub == type);
}

// the caller should make sure there is a document object
const TCHAR *Doc::GetFilePathFromDoc() const
{
    switch (type) {
    case Engine:
        return engine->FileName();
    case Epub:
        return epubDoc->GetFileName();
    case Error:
        return NULL;
    case Mobi:
        return mobiDoc->GetFileName();
    case MobiTest:
        return NULL;
    case None:
        return NULL;
    default:
        CrashIf(true);
        return NULL;
    }
}

const TCHAR *Doc::GetFilePath() const
{
    if (filePath) {
        // verify it's consistent with the path in the doc
        const TCHAR *docPath = GetFilePathFromDoc();
        CrashIf(docPath && !str::Eq(filePath, docPath));
        return filePath;
    }
    CrashIf(!generic && !(IsNone() || IsError()));
    return GetFilePathFromDoc();
}

TCHAR *Doc::GetProperty(const char *name)
{
    if (Epub == type)
        return epubDoc->GetProperty(name);
    if (Engine == type)
        return engine->GetProperty(name);
    return NULL;
}

const char *Doc::GetHtmlData(size_t &len)
{
    switch (type) {
    case Mobi:
        return mobiDoc->GetBookHtmlData(len);
    case MobiTest:
        return mobiTestDoc->GetBookHtmlData(len);
    case Epub:
        return epubDoc->GetTextData(&len);
    default:
        CrashIf(true);
        return NULL;
    }
}

size_t Doc::GetHtmlDataSize()
{
    switch (type) {
    case Mobi:
        return mobiDoc->GetBookHtmlSize();
    case MobiTest:
        return mobiTestDoc->GetBookHtmlSize();
    case Epub:
        return epubDoc->GetTextDataSize();
    default:
        CrashIf(true);
        return NULL;
    }
}

ImageData *Doc::GetCoverImage()
{
    if (type != Mobi)
        return NULL;
    return mobiDoc->GetCoverImage();
}

Doc Doc::CreateFromFile(const TCHAR *filePath)
{
    Doc doc;
    if (MobiDoc::IsSupportedFile(filePath))
        doc = Doc(MobiDoc::CreateFromFile(filePath));
    else if (EpubDoc::IsSupportedFile(filePath))
        doc = Doc(EpubDoc::CreateFromFile(filePath));

    doc.filePath = str::Dup(filePath);
    // if failed to load and more specific error message hasn't been
    // set above, set a generic error message
    if ((NULL == doc.generic) && (NULL == doc.loadingErrorMessage)) {
        doc.type = Error;
        doc.loadingErrorMessage = str::Format(_T("Error loading %s"), filePath);
    }
    return doc;
}

bool EngineManager::IsSupportedFile(const TCHAR *filePath, bool sniff)
{
    return PdfEngine::IsSupportedFile(filePath, sniff)  ||
           XpsEngine::IsSupportedFile(filePath, sniff)  ||
           DjVuEngine::IsSupportedFile(filePath, sniff) ||
           CbxEngine::IsSupportedFile(filePath, sniff)  ||
           ImageEngine::IsSupportedFile(filePath, sniff)||
           ImageDirEngine::IsSupportedFile(filePath, sniff) ||
           PsEngine::IsSupportedFile(filePath, sniff)   ||
           ChmEngine::IsSupportedFile(filePath, sniff)  ||
           enableEbookEngines && (
               EpubEngine::IsSupportedFile(filePath, sniff) ||
               Fb2Engine::IsSupportedFile(filePath, sniff)  ||
               MobiEngine::IsSupportedFile(filePath, sniff) ||
               PdbEngine::IsSupportedFile(filePath, sniff)  ||
               Chm2Engine::IsSupportedFile(filePath, sniff) ||
               HtmlEngine::IsSupportedFile(filePath, sniff) ||
               TxtEngine::IsSupportedFile(filePath, sniff)
           );
}

BaseEngine *EngineManager::CreateEngine(const TCHAR *filePath, PasswordUI *pwdUI, EngineType *typeOut)
{
    assert(filePath);
    if (!filePath) return NULL;

    BaseEngine *engine = NULL;
    EngineType engineType = Engine_None;
    bool sniff = false;
RetrySniffing:
    if (PdfEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_PDF) {
        engine = PdfEngine::CreateFromFile(filePath, pwdUI);
        engineType = Engine_PDF;
    } else if (XpsEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_XPS) {
        engine = XpsEngine::CreateFromFile(filePath);
        engineType = Engine_XPS;
    } else if (DjVuEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_DjVu) {
        engine = DjVuEngine::CreateFromFile(filePath);
        engineType = Engine_DjVu;
    } else if (CbxEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ComicBook) {
        engine = CbxEngine::CreateFromFile(filePath);
        engineType = Engine_ComicBook;
    } else if (ImageEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Image) {
        engine = ImageEngine::CreateFromFile(filePath);
        engineType = Engine_Image;
    } else if (ImageDirEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ImageDir) {
        engine = ImageDirEngine::CreateFromFile(filePath);
        engineType = Engine_ImageDir;
    } else if (PsEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_PS) {
        engine = PsEngine::CreateFromFile(filePath);
        engineType = Engine_PS;
    } else if (ChmEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Chm) {
        engine = ChmEngine::CreateFromFile(filePath);
        engineType = Engine_Chm;
    } else if (!enableEbookEngines) {
        // don't try to create any of the below ebook engines
    } else if (EpubEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Epub) {
        engine = EpubEngine::CreateFromFile(filePath);
        engineType = Engine_Epub;
    } else if (Fb2Engine::IsSupportedFile(filePath, sniff) && engineType != Engine_Fb2) {
        engine = Fb2Engine::CreateFromFile(filePath);
        engineType = Engine_Fb2;
    } else if (MobiEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Mobi) {
        engine = MobiEngine::CreateFromFile(filePath);
        engineType = Engine_Mobi;
    } else if (PdbEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Pdb) {
        engine = PdbEngine::CreateFromFile(filePath);
        engineType = Engine_Pdb;
    } else if (Chm2Engine::IsSupportedFile(filePath, sniff) && engineType != Engine_Chm2) {
        engine = Chm2Engine::CreateFromFile(filePath);
        engineType = Engine_Chm2;
    } else if (HtmlEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Html) {
        engine = HtmlEngine::CreateFromFile(filePath);
        engineType = Engine_Html;
    } else if (TxtEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Txt) {
        engine = TxtEngine::CreateFromFile(filePath);
        engineType = Engine_Txt;
    }

    if (!engine && !sniff) {
        // try sniffing the file instead
        sniff = true;
        goto RetrySniffing;
    }

    if (typeOut)
        *typeOut = engine ? engineType : Engine_None;
    return engine;
}
