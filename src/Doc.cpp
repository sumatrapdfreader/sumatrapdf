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
    case None:
        break;
    default:
        if (AsEngine())
            delete AsEngine();
        else
            CrashIf(true);
        break;
    }

    type = None;
    dummy = NULL;
}

BaseEngine *Doc::AsEngine() const
{
    switch (type) {
    case None:
    case Mobi:
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

EpubDoc *Doc::AsEpub() const
{
    if (Epub == type)
        return epubDoc;
    return NULL;
}

// return true if this is document supported by ebook UI
bool Doc::IsEbook() const
{
    return (Mobi == type) || (Epub == type);
}

TCHAR *Doc::GetFilePath() const
{
    switch (type) {
    case Mobi:
        return mobiDoc->GetFileName();
    default:
        CrashIf(true);
        break;
    }
    return NULL;
}

