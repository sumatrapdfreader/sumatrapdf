/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "ArchUtil.h"
#include "HtmlParserLookup.h"
#include "HtmlPullParser.h"
#include "Mui.h"
// rendering engines
#include "BaseEngine.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"
#include "Doc.h"

Doc::Doc(const Doc& other)
{
    Clear();
    type = other.type;
    generic = other.generic;
    error = other.error;
    filePath.SetCopy(other.filePath);
}

Doc& Doc::operator=(const Doc& other)
{
    if (this != &other) {
        type = other.type;
        generic = other.generic;
        error = other.error;
        filePath.SetCopy(other.filePath);
    }
    return *this;
}

Doc::~Doc()
{
}

// delete underlying object
void Doc::Delete()
{
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

Doc::Doc(EpubDoc *doc)
{
    Clear();
    type = doc ? DocType::Epub : DocType::None;
    epubDoc = doc;
}

Doc::Doc(Fb2Doc *doc)
{
    Clear();
    type = doc ? DocType::Fb2 : DocType::None;
    fb2Doc = doc;
}

Doc::Doc(MobiDoc *doc)
{
    Clear();
    type = doc ? DocType::Mobi : DocType::None;
    mobiDoc = doc;
}

Doc::Doc(PalmDoc *doc)
{
    Clear();
    type = doc ? DocType::Pdb : DocType::None;
    palmDoc = doc;
}

void Doc::Clear()
{
    type = DocType::None;
    generic = nullptr;
    error = Error_None;
    filePath.Set(nullptr);
}

// the caller should make sure there is a document object
const WCHAR *Doc::GetFilePathFromDoc() const
{
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

const WCHAR *Doc::GetDefaultFileExt() const
{
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

WCHAR *Doc::GetProperty(DocumentProperty prop) const
{
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

const char *Doc::GetHtmlData(size_t &len) const
{
    switch (type) {
    case DocType::Epub:
        return epubDoc->GetHtmlData(&len);
    case DocType::Fb2:
        return fb2Doc->GetXmlData(&len);
    case DocType::Mobi:
        return mobiDoc->GetHtmlData(len);
    case DocType::Pdb:
        return palmDoc->GetHtmlData(&len);
    default:
        CrashIf(true);
        return nullptr;
    }
}

size_t Doc::GetHtmlDataSize() const
{
    switch (type) {
    case DocType::Epub:
        return epubDoc->GetHtmlDataSize();
    case DocType::Fb2:
        return fb2Doc->GetXmlDataSize();
    case DocType::Mobi:
        return mobiDoc->GetHtmlDataSize();
    case DocType::Pdb:
        return palmDoc->GetHtmlDataSize();
    default:
        CrashIf(true);
        return 0;
    }
}

ImageData *Doc::GetCoverImage() const
{
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

bool Doc::HasToc() const
{
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

bool Doc::ParseToc(EbookTocVisitor *visitor) const
{
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

HtmlFormatter *Doc::CreateFormatter(HtmlFormatterArgs *args) const
{
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

Doc Doc::CreateFromFile(const WCHAR *filePath)
{
    Doc doc;
    if (EpubDoc::IsSupportedFile(filePath))
        doc = Doc(EpubDoc::CreateFromFile(filePath));
    else if (Fb2Doc::IsSupportedFile(filePath))
        doc = Doc(Fb2Doc::CreateFromFile(filePath));
    else if (MobiDoc::IsSupportedFile(filePath)) {
        doc = Doc(MobiDoc::CreateFromFile(filePath));
        // MobiDoc is also used for loading PalmDoc - don't expose that to Doc users, though
        if (doc.mobiDoc && doc.mobiDoc->GetDocType() != Pdb_Mobipocket) {
            doc.Delete();
            // .prc files can be both MobiDoc or PalmDoc
            if (PalmDoc::IsSupportedFile(filePath))
                doc = Doc(PalmDoc::CreateFromFile(filePath));
        }
    }
    else if (PalmDoc::IsSupportedFile(filePath))
        doc = Doc(PalmDoc::CreateFromFile(filePath));

    // if failed to load and more specific error message hasn't been
    // set above, set a generic error message
    if (doc.IsNone()) {
        CrashIf(doc.error);
        doc.error = Error_Unknown;
        doc.filePath.SetCopy(filePath);
    }
    else {
        CrashIf(!Doc::IsSupportedFile(filePath));
    }
    CrashIf(!doc.generic && !doc.IsNone());
    return doc;
}

bool Doc::IsSupportedFile(const WCHAR *filePath, bool sniff)
{
    return EpubDoc::IsSupportedFile(filePath, sniff) ||
           Fb2Doc::IsSupportedFile(filePath, sniff) ||
           MobiDoc::IsSupportedFile(filePath, sniff) ||
           PalmDoc::IsSupportedFile(filePath, sniff);
}
