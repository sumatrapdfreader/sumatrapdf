/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Doc.h"

#include "BaseEngine.h"
#include "ChmEngine.h"
#include "DjVuEngine.h"
#include "EbookEngine.h"
#include "ImagesEngine.h"
#include "PdfEngine.h"
#include "PsEngine.h"

#include "EbookDoc.h"
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
    case Doc_Epub:
        delete epubDoc;
        break;
    case Doc_Mobi:
        delete mobiDoc;
        break;
    case Doc_MobiTest:
        delete mobiTestDoc;
        break;
    case Doc_None:
        break;
    default:
        CrashIf(!IsEngine());
        delete engine;
        break;
    }

    FreeStrings();
    Clear();
}

Doc::Doc(BaseEngine *doc, DocType engineType)
{
    Clear();
    type = doc ? engineType : Engine_None;
    engine = doc;
    CrashIf(doc && !IsEngine());
}

Doc::Doc(EpubDoc *doc)
{
    Clear();
    type = doc ? Doc_Epub : Doc_None;
    epubDoc = doc;
}

Doc::Doc(MobiDoc *doc)
{
    Clear();
    type = doc ? Doc_Mobi : Doc_None;
    mobiDoc = doc;
}

Doc::Doc(MobiTestDoc *doc)
{ 
    Clear();
    type = doc ? Doc_MobiTest : Doc_None;
    mobiTestDoc = doc;
}

void Doc::Clear()
{
    type = Doc_None;
    loadingErrorMessage = NULL;
    filePath = NULL;
    generic = NULL;
}

BaseEngine *Doc::AsEngine() const
{
    if (IsEngine())
        return engine;
    return NULL;
}

MobiDoc *Doc::AsMobi() const
{
    if (Doc_Mobi == type)
        return mobiDoc;
    return NULL;
}

MobiTestDoc *Doc::AsMobiTest() const
{
    if (Doc_MobiTest == type)
        return mobiTestDoc;
    return NULL;
}

EpubDoc *Doc::AsEpub() const
{
    if (Doc_Epub == type)
        return epubDoc;
    return NULL;
}

// return true if this is document supported by ebook UI
bool Doc::IsEbook() const
{
    return (Doc_Mobi == type) || (Doc_MobiTest == type) || (Doc_Epub == type);
}

bool Doc::IsEngine() const
{
    switch (type) {
    case Engine_PDF: case Engine_XPS:
    case Engine_DjVu:
    case Engine_Image: case Engine_ImageDir: case Engine_ComicBook:
    case Engine_PS:
    case Engine_Chm:
    case Engine_Epub: case Engine_Fb2: case Engine_Mobi: case Engine_Pdb:
    case Engine_Chm2: case Engine_Tcr: case Engine_Html: case Engine_Txt:
        return true;
    default:
        CrashIf(!IsEbook() && !IsNone());
        return false;
    }
}

// the caller should make sure there is a document object
const WCHAR *Doc::GetFilePathFromDoc() const
{
    switch (type) {
    case Doc_Epub:
        return epubDoc->GetFileName();
    case Doc_Mobi:
        return mobiDoc->GetFileName();
    case Doc_MobiTest:
        return NULL;
    case Doc_None:
        return NULL;
    default:
        CrashIf(!IsEngine());
        return engine->FileName();
    }
}

const WCHAR *Doc::GetFilePath() const
{
    if (filePath) {
        // verify it's consistent with the path in the doc
        const WCHAR *docPath = GetFilePathFromDoc();
        CrashIf(docPath && !str::Eq(filePath, docPath));
        return filePath;
    }
    CrashIf(!generic && !IsNone());
    return GetFilePathFromDoc();
}

WCHAR *Doc::GetProperty(DocumentProperty prop)
{
    if (Doc_Mobi == type)
        return mobiDoc->GetProperty(prop);
    if (Doc_Epub == type)
        return epubDoc->GetProperty(prop);
    if (IsEngine())
        return engine->GetProperty(prop);
    return NULL;
}

const char *Doc::GetHtmlData(size_t &len)
{
    switch (type) {
    case Doc_Mobi:
        return mobiDoc->GetBookHtmlData(len);
    case Doc_MobiTest:
        return mobiTestDoc->GetBookHtmlData(len);
    case Doc_Epub:
        return epubDoc->GetTextData(&len);
    default:
        CrashIf(true);
        return NULL;
    }
}

size_t Doc::GetHtmlDataSize()
{
    switch (type) {
    case Doc_Mobi:
        return mobiDoc->GetBookHtmlSize();
    case Doc_MobiTest:
        return mobiTestDoc->GetBookHtmlSize();
    case Doc_Epub:
        return epubDoc->GetTextDataSize();
    default:
        CrashIf(true);
        return NULL;
    }
}

ImageData *Doc::GetCoverImage()
{
    if (type != Doc_Mobi)
        return NULL;
    return mobiDoc->GetCoverImage();
}

Doc Doc::CreateFromFile(const WCHAR *filePath)
{
    Doc doc;
    if (MobiDoc::IsSupportedFile(filePath))
        doc = Doc(MobiDoc::CreateFromFile(filePath));
    else if (EpubDoc::IsSupportedFile(filePath))
        doc = Doc(EpubDoc::CreateFromFile(filePath));

    doc.filePath = str::Dup(filePath);
    // if failed to load and more specific error message hasn't been
    // set above, set a generic error message
    if (doc.IsNone()) {
        CrashIf(doc.loadingErrorMessage);
        doc.loadingErrorMessage = str::Format(L"Error loading %s", filePath);
    }
    CrashIf(!doc.generic && !doc.IsNone());
    return doc;
}

namespace EngineManager {

bool IsSupportedFile(bool enableEbookEngines, const WCHAR *filePath, bool sniff)
{
    return PdfEngine::IsSupportedFile(filePath, sniff)  ||
           XpsEngine::IsSupportedFile(filePath, sniff)  ||
           DjVuEngine::IsSupportedFile(filePath, sniff) ||
           ImageEngine::IsSupportedFile(filePath, sniff)||
           ImageDirEngine::IsSupportedFile(filePath, sniff) ||
           CbxEngine::IsSupportedFile(filePath, sniff)  ||
           PsEngine::IsSupportedFile(filePath, sniff)   ||
           ChmEngine::IsSupportedFile(filePath, sniff)  ||
           enableEbookEngines && (
               EpubEngine::IsSupportedFile(filePath, sniff) ||
               Fb2Engine::IsSupportedFile(filePath, sniff)  ||
               MobiEngine::IsSupportedFile(filePath, sniff) ||
               PdbEngine::IsSupportedFile(filePath, sniff)  ||
               Chm2Engine::IsSupportedFile(filePath, sniff) ||
               TcrEngine::IsSupportedFile(filePath, sniff)  ||
               HtmlEngine::IsSupportedFile(filePath, sniff) ||
               TxtEngine::IsSupportedFile(filePath, sniff)
           );
}

BaseEngine *CreateEngine(bool enableEbookEngines, const WCHAR *filePath, PasswordUI *pwdUI, DocType *typeOut)
{
    CrashIf(!filePath);

    BaseEngine *engine = NULL;
    DocType engineType = Engine_None;
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
    } else if (ImageEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Image) {
        engine = ImageEngine::CreateFromFile(filePath);
        engineType = Engine_Image;
    } else if (ImageDirEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ImageDir) {
        engine = ImageDirEngine::CreateFromFile(filePath);
        engineType = Engine_ImageDir;
    } else if (CbxEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ComicBook) {
        engine = CbxEngine::CreateFromFile(filePath);
        engineType = Engine_ComicBook;
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
    } else if (TcrEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Tcr) {
        engine = TcrEngine::CreateFromFile(filePath);
        engineType = Engine_Tcr;
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
    CrashIf(engine && !IsSupportedFile(enableEbookEngines, filePath, sniff));

    if (typeOut)
        *typeOut = engine ? engineType : Engine_None;
    return engine;
}

} // namespace EngineManager
