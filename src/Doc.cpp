/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Doc.h"
#include "EngineManager.h"
#include "MobiDoc.h"
#include "EpubDoc.h"

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

const TCHAR *Doc::GetFilePath() const
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
    if (MobiDoc::IsSupportedFile(filePath))
        return Doc(MobiDoc::CreateFromFile(filePath));
    if (EpubDoc::IsSupportedFile(filePath))
        return Doc(EpubDoc::CreateFromFile(filePath));
    return Doc();
}
