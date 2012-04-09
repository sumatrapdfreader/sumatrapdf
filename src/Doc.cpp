/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Doc.h"

#include "EngineManager.h"
#include "EpubDoc.h"
#include "MobiDoc.h"
#include "Translations.h"

Doc::Doc(const Doc& other)
{
    type = other.type;
    generic = other.generic;
    loadingErrorMessage = NULL;
    filePath = NULL;
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

void Doc::FreeStrings()
{
    str::ReplacePtr(&loadingErrorMessage, NULL);
    str::ReplacePtr(&filePath, NULL);
}

// delete underlying object
void Doc::Delete()
{
    switch (type) {
    case Mobi:
        delete mobiDoc;
        break;
    case Epub:
        delete epubDoc;
        break;
    case MobiTest:
        delete mobiTestDoc;
        break;
    case None:
        break;
    default:
        if (IsEngine())
            delete engine;
        else
            CrashIf(true);
        break;
    }

    type = None;
    generic = NULL;
    FreeStrings();
}

void Doc::SetEngine(DocType newType, BaseEngine *doc)
{
    type = newType;
    engine = doc;
    FreeStrings();
}

void Doc::Set(CbxEngine *doc)      { SetEngine(CbxEng,      doc); }
void Doc::Set(ChmEngine *doc)      { SetEngine(ChmEng,      doc); }
void Doc::Set(Chm2Engine *doc)     { SetEngine(Chm2Eng,     doc); }
void Doc::Set(DjVuEngine *doc)     { SetEngine(DjVuEng,     doc); }
void Doc::Set(EpubEngine *doc)     { SetEngine(EpubEng,     doc); }
void Doc::Set(Fb2Engine *doc)      { SetEngine(Fb2Eng,      doc); }
void Doc::Set(ImageEngine *doc)    { SetEngine(ImageEng,    doc); }
void Doc::Set(ImageDirEngine *doc) { SetEngine(ImageDirEng, doc); }
void Doc::Set(MobiEngine *doc)     { SetEngine(MobiEng,     doc); }
void Doc::Set(PdfEngine *doc)      { SetEngine(PdfEng,      doc); }
void Doc::Set(PsEngine *doc)       { SetEngine(PsEng,       doc);}
void Doc::Set(XpsEngine *doc)      { SetEngine(XpsEng,      doc); }

void Doc::Set(EpubDoc *doc)
{
    type = Epub;
    epubDoc = doc;
    FreeStrings();
}

void Doc::Set(MobiDoc *doc)
{
    type = Mobi;
    mobiDoc = doc;
    FreeStrings();
}

void Doc::Set(MobiTestDoc *doc)
{ 
    type = MobiTest;
    mobiTestDoc = doc;
    FreeStrings();
}

BaseEngine *Doc::AsEngine() const
{
    switch (type) {
    case None:
    case Mobi:
    case MobiTest:
    case Epub:
        return NULL;
    }
    return engine;
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
    case Mobi:
        return mobiDoc->GetFileName();
    case MobiTest:
        return NULL;
    case Epub:
        return epubDoc->GetFileName();
    case None:
        return NULL;
    default:
        CrashIf(!engine);
        return engine->FileName();
    }
}

const TCHAR *Doc::GetFilePath() const
{
    if (filePath) {
        // verify it's consistent with the path in the doc
        if (NULL != generic) {
            const TCHAR *docPath = GetFilePathFromDoc();
            CrashIf(docPath && !str::Eq(filePath, docPath));
        }
        return filePath;
    }
    if (NULL == generic) {
        CrashIf(None != type);
        return NULL;
    }
    return GetFilePathFromDoc();
}

TCHAR *Doc::GetProperty(const char *name)
{
    if (Epub == type)
        return epubDoc->GetProperty(name);
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
        doc.Set(MobiDoc::CreateFromFile(filePath));
    else if (EpubDoc::IsSupportedFile(filePath))
        doc.Set(EpubDoc::CreateFromFile(filePath));

    doc.filePath = str::Dup(filePath);
    // if failed to load and more specific error message hasn't been
    // set above, set a generic error message
    if ((NULL == doc.generic) && (NULL == doc.loadingErrorMessage)) {
        doc.loadingErrorMessage = str::Format(_TR("Error loading %s"), filePath);
    }
    return doc;
}
