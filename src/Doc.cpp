/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Archive.h"
#include "utils/PalmDbReader.h"
#include "utils/GuessFileType.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "mui/Mui.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"
#include "Doc.h"

Doc::Doc(const Doc& other) {
    Clear();
    type = other.type;
    generic = other.generic;
    error = other.error;
    filePath.SetCopy(other.filePath);
}

Doc& Doc::operator=(const Doc& other) {
    if (this != &other) {
        type = other.type;
        generic = other.generic;
        error = other.error;
        filePath.SetCopy(other.filePath);
    }
    return *this;
}

Doc::~Doc() {
}

// delete underlying object
void Doc::Delete() {
    switch (type) {
        case DocType::Epub:
            delete epubDoc;
            break;
        case DocType::Fb2:
            delete fb2Doc;
            break;
        case DocType::Mobi:
            delete mobiDoc;
            break;
        case DocType::Pdb:
            delete palmDoc;
            break;
        case DocType::None:
            break;
        default:
            CrashIf(true);
            break;
    }

    Clear();
}

Doc::Doc(EpubDoc* doc) {
    Clear();
    if (doc == nullptr) {
        return;
    }
    type = DocType::Epub;
    epubDoc = doc;
}

Doc::Doc(Fb2Doc* doc) {
    Clear();
    if (doc == nullptr) {
        return;
    }
    type = DocType::Fb2;
    fb2Doc = doc;
}

Doc::Doc(MobiDoc* doc) {
    Clear();
    if (doc == nullptr) {
        return;
    }
    type = DocType::Mobi;
    mobiDoc = doc;
}

Doc::Doc(PalmDoc* doc) {
    Clear();
    if (doc == nullptr) {
        return;
    }
    type = DocType::Pdb;
    palmDoc = doc;
}

void Doc::Clear() {
    type = DocType::None;
    generic = nullptr;
    error = DocError::None;
    filePath.Reset();
}

// the caller should make sure there is a document object
const WCHAR* Doc::GetFilePathFromDoc() const {
    switch (type) {
        case DocType::Epub:
            return epubDoc->GetFileName();
        case DocType::Fb2:
            return fb2Doc->GetFileName();
        case DocType::Mobi:
            return mobiDoc->GetFileName();
        case DocType::Pdb:
            return palmDoc->GetFileName();
        case DocType::None:
            return nullptr;
        default:
            CrashIf(true);
            return nullptr;
    }
}

const WCHAR* Doc::GetFilePath() const {
    if (filePath) {
        // verify it's consistent with the path in the doc
        const WCHAR* docPath = GetFilePathFromDoc();
        CrashIf(docPath && !str::Eq(filePath, docPath));
        return filePath;
    }
    CrashIf(!generic && !IsNone());
    return GetFilePathFromDoc();
}

const WCHAR* Doc::GetDefaultFileExt() const {
    switch (type) {
        case DocType::Epub:
            return L".epub";
        case DocType::Fb2:
            return fb2Doc->IsZipped() ? L".fb2z" : L".fb2";
        case DocType::Mobi:
            return L".mobi";
        case DocType::Pdb:
            return L".pdb";
        case DocType::None:
            return nullptr;
        default:
            CrashIf(true);
            return nullptr;
    }
}

WCHAR* Doc::GetProperty(DocumentProperty prop) const {
    switch (type) {
        case DocType::Epub:
            return epubDoc->GetProperty(prop);
        case DocType::Fb2:
            return fb2Doc->GetProperty(prop);
        case DocType::Mobi:
            return mobiDoc->GetProperty(prop);
        case DocType::Pdb:
            return palmDoc->GetProperty(prop);
        case DocType::None:
            return nullptr;
        default:
            CrashIf(true);
            return nullptr;
    }
}

std::span<u8> Doc::GetHtmlData() const {
    switch (type) {
        case DocType::Epub:
            return epubDoc->GetHtmlData();
        case DocType::Fb2:
            return fb2Doc->GetXmlData();
        case DocType::Mobi: {
            return mobiDoc->GetHtmlData();
        }
        case DocType::Pdb:
            return palmDoc->GetHtmlData();
        default:
            CrashIf(true);
            return {};
    }
}

ImageData* Doc::GetCoverImage() const {
    switch (type) {
        case DocType::Fb2:
            return fb2Doc->GetCoverImage();
        case DocType::Mobi:
            return mobiDoc->GetCoverImage();
        case DocType::Epub:
        case DocType::Pdb:
        default:
            return nullptr;
    }
}

bool Doc::HasToc() const {
    switch (type) {
        case DocType::Epub:
            return epubDoc->HasToc();
        case DocType::Fb2:
            return fb2Doc->HasToc();
        case DocType::Mobi:
            return mobiDoc->HasToc();
        case DocType::Pdb:
            return palmDoc->HasToc();
        default:
            return false;
    }
}

bool Doc::ParseToc(EbookTocVisitor* visitor) const {
    switch (type) {
        case DocType::Epub:
            return epubDoc->ParseToc(visitor);
        case DocType::Fb2:
            return fb2Doc->ParseToc(visitor);
        case DocType::Mobi:
            return mobiDoc->ParseToc(visitor);
        case DocType::Pdb:
            return palmDoc->ParseToc(visitor);
        default:
            return false;
    }
}

HtmlFormatter* Doc::CreateFormatter(HtmlFormatterArgs* args) const {
    switch (type) {
        case DocType::Epub:
            return new EpubFormatter(args, epubDoc);
        case DocType::Fb2:
            return new Fb2Formatter(args, fb2Doc);
        case DocType::Mobi:
            return new MobiFormatter(args, mobiDoc);
        case DocType::Pdb:
            return new HtmlFormatter(args);
        default:
            CrashIf(true);
            return nullptr;
    }
}

Doc Doc::CreateFromFile(const WCHAR* path) {
    Doc doc;
    bool sniff = false;
again:
    Kind kind = GuessFileType(path, sniff);
    if (EpubDoc::IsSupportedFileType(kind)) {
        doc = Doc(EpubDoc::CreateFromFile(path));
    } else if (Fb2Doc::IsSupportedFileType(kind)) {
        doc = Doc(Fb2Doc::CreateFromFile(path));
    } else if (MobiDoc::IsSupportedFileType(kind)) {
        doc = Doc(MobiDoc::CreateFromFile(path));
    } else if (PalmDoc::IsSupportedFileType(kind)) {
        doc = Doc(PalmDoc::CreateFromFile(path));
    } else {
        doc.error = DocError::NotSupported;
    }
    if (!sniff) {
        if (doc.IsNone()) {
            sniff = true;
            goto again;
        }
    }

    // if failed to load and more specific error message hasn't been
    // set above, set a generic error message
    if (doc.IsNone()) {
        CrashIf(!((doc.error == DocError::None) || (doc.error == DocError::NotSupported)));
        doc.error = DocError::Unknown;
        doc.filePath.SetCopy(path);
    }
    CrashIf(!doc.generic && !doc.IsNone());
    return doc;
}

bool Doc::IsSupportedFileType(Kind kind) {
    if (EpubDoc::IsSupportedFileType(kind)) {
        return true;
    }
    if (Fb2Doc::IsSupportedFileType(kind)) {
        return true;
    }
    if (MobiDoc::IsSupportedFileType(kind)) {
        return true;
    }
    if (PalmDoc::IsSupportedFileType(kind)) {
        return true;
    }
    return false;
}
