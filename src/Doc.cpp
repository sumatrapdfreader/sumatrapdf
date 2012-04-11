/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Doc.h"

#include "EngineManager.h"
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
