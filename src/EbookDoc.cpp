/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Archive.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"

// tries to extract an encoding from <?xml encoding="..."?>
// returns CP_ACP on failure
static uint GetCodepageFromPI(const char* xmlPI) {
    if (!str::StartsWith(xmlPI, "<?xml")) {
        return CP_ACP;
    }
    const char* xmlPIEnd = str::Find(xmlPI, "?>");
    if (!xmlPIEnd) {
        return CP_ACP;
    }
    HtmlToken pi;
    pi.SetTag(HtmlToken::EmptyElementTag, xmlPI + 2, xmlPIEnd);
    pi.nLen = 4;
    AttrInfo* enc = pi.GetAttrByName("encoding");
    if (!enc) {
        return CP_ACP;
    }

    TempStr encoding = str::DupTemp(enc->val, enc->valLen);
    struct {
        const char* namePart;
        uint codePage;
    } static encodings[] = {
        {"UTF", CP_UTF8}, {"utf", CP_UTF8}, {"1252", 1252}, {"1251", 1251},
        // TODO: any other commonly used codepages?
    };
    for (size_t i = 0; i < dimof(encodings); i++) {
        if (str::Find(encoding, encodings[i].namePart)) {
            return encodings[i].codePage;
        }
    }
    return CP_ACP;
}

static bool IsValidUtf8(const char* string) {
    for (const u8* s = (const u8*)string; *s; s++) {
        int skip;
        if (*s < 0x80) {
            skip = 0;
        } else if (*s < 0xC0) {
            return false;
        } else if (*s < 0xE0) {
            skip = 1;
        } else if (*s < 0xF0) {
            skip = 2;
        } else if (*s < 0xF5) {
            skip = 3;
        } else {
            return false;
        }
        while (skip-- > 0) {
            if ((*++s & 0xC0) != 0x80) {
                return false;
            }
        }
    }
    return true;
}

static char* DecodeTextToUtf8(const char* s, bool isXML = false) {
    AutoFree tmp;
    if (str::StartsWith(s, UTF16BE_BOM)) {
        size_t byteCount = (str::Len((WCHAR*)s) + 1) * sizeof(WCHAR);
        tmp.Set((char*)memdup(s, byteCount));
        for (size_t i = 0; i + 1 < byteCount; i += 2) {
            std::swap(tmp[i], tmp[i + 1]);
        }
        s = tmp;
    }
    if (str::StartsWith(s, UTF16_BOM)) {
        char* tmp2 = ToUtf8((WCHAR*)(s + 2));
        return tmp2;
    }
    if (str::StartsWith(s, UTF8_BOM)) {
        return str::Dup(s + 3);
    }
    uint codePage = isXML ? GetCodepageFromPI(s) : CP_ACP;
    if (CP_ACP == codePage && IsValidUtf8(s)) {
        return str::Dup(s);
    }
    if (CP_ACP == codePage) {
        codePage = GuessTextCodepage(s, str::Len(s), CP_ACP);
    }
    return strconv::ToMultiByte(s, codePage, CP_UTF8);
}

char* NormalizeURL(const char* url, const char* base) {
    CrashIf(!url || !base);
    if (*url == '/' || str::FindChar(url, ':')) {
        return str::Dup(url);
    }

    const char* baseEnd = str::FindCharLast(base, '/');
    const char* hash = str::FindChar(base, '#');
    if (*url == '#') {
        baseEnd = hash ? hash - 1 : base + str::Len(base) - 1;
    } else if (baseEnd && hash && hash < baseEnd) {
        for (baseEnd = hash - 1; baseEnd > base && *baseEnd != '/'; baseEnd--) {
            ;
        }
    }
    if (baseEnd) {
        baseEnd++;
    } else {
        baseEnd = base;
    }
    AutoFreeStr basePath = str::Dup(base, baseEnd - base);
    AutoFreeStr norm = str::Join(basePath, url);

    char* dst = norm;
    for (char* src = norm; *src; src++) {
        if (*src != '/') {
            *dst++ = *src;
        } else if (str::StartsWith(src, "/./")) {
            src++;
        } else if (str::StartsWith(src, "/../")) {
            for (; dst > norm && *(dst - 1) != '/'; dst--) {
                ;
            }
            src += 3;
        } else {
            *dst++ = '/';
        }
    }
    *dst = '\0';
    return norm.Release();
}

inline char decode64(char c) {
    if ('A' <= c && c <= 'Z') {
        return c - 'A';
    }
    if ('a' <= c && c <= 'z') {
        return c - 'a' + 26;
    }
    if ('0' <= c && c <= '9') {
        return c - '0' + 52;
    }
    if ('+' == c) {
        return 62;
    }
    if ('/' == c) {
        return 63;
    }
    return -1;
}

static ByteSlice Base64Decode(const ByteSlice& data) {
    size_t sLen = data.size();
    u8* s = data.data();
    u8* end = data.data() + sLen;
    u8* result = AllocArray<u8>(sLen * 3 / 4);
    u8* curr = result;
    u8 c = 0;
    int step = 0;
    for (; s < end && *s != '='; s++) {
        char n = decode64(*s);
        if (-1 == n) {
            if (str::IsWs((char)*s)) {
                continue;
            }
            free(result);
            return {};
        }
        switch (step++ % 4) {
            case 0:
                c = n;
                break;
            case 1:
                *curr++ = (c << 2) | (n >> 4);
                c = n & 0xF;
                break;
            case 2:
                *curr++ = (c << 4) | (n >> 2);
                c = n & 0x3;
                break;
            case 3:
                *curr++ = (c << 6) | (n >> 0);
                break;
        }
    }
    size_t size = curr - result;
    return {(u8*)result, size};
}

static inline void AppendChar(str::Str& htmlData, char c) {
    switch (c) {
        case '&':
            htmlData.Append("&amp;");
            break;
        case '<':
            htmlData.Append("&lt;");
            break;
        case '"':
            htmlData.Append("&quot;");
            break;
        default:
            htmlData.AppendChar(c);
            break;
    }
}

// the caller must free
static ByteSlice DecodeDataURI(const char* url) {
    const char* comma = str::FindChar(url, ',');
    if (!comma) {
        return {};
    }
    const char* data = comma + 1;
    if (comma - url >= 12 && str::EqN(comma - 7, ";base64", 7)) {
        ByteSlice d{(u8*)data, str::Len(data)};
        return Base64Decode(d);
    }
    return {(u8*)str::Dup(data), str::Len(data)};
}

int PropertyMap::Find(DocumentProperty prop) const {
    int n = static_cast<int>(prop);
    if ((n >= 0) && (n < (int)dimof(values))) {
        return n;
    }
    return -1;
}

void PropertyMap::Set(DocumentProperty prop, char* valueUtf8, bool replace) {
    int idx = Find(prop);
    CrashIf(-1 == idx);
    if (-1 == idx || !replace && values[idx]) {
        str::Free(valueUtf8);
    } else {
        values[idx].Set(valueUtf8);
    }
}

char* PropertyMap::Get(DocumentProperty prop) const {
    int idx = Find(prop);
    if (idx >= 0 && values[idx]) {
        return str::Dup(values[idx].Get());
    }
    return nullptr;
}

/* ********** EPUB ********** */

const char* EPUB_CONTAINER_NS = "urn:oasis:names:tc:opendocument:xmlns:container";
const char* EPUB_OPF_NS = "http://www.idpf.org/2007/opf";
const char* EPUB_NCX_NS = "http://www.daisy.org/z3986/2005/ncx/";
const char* EPUB_ENC_NS = "http://www.w3.org/2001/04/xmlenc#";

EpubDoc::EpubDoc(const char* fileName) {
    this->fileName.SetCopy(fileName);
    InitializeCriticalSection(&zipAccess);
    zip = OpenZipArchive(fileName, true);
}

EpubDoc::EpubDoc(IStream* stream) {
    InitializeCriticalSection(&zipAccess);
    zip = OpenZipArchive(stream, true);
}

EpubDoc::~EpubDoc() {
    EnterCriticalSection(&zipAccess);

    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName);
    }

    LeaveCriticalSection(&zipAccess);
    DeleteCriticalSection(&zipAccess);
    delete zip;
}

// TODO: switch to seqstring
static bool isHtmlMediaType(const char* mediatype) {
    if (str::Eq(mediatype, "application/xhtml+xml")) {
        return true;
    }
    if (str::Eq(mediatype, "application/html+xml")) {
        return true;
    }
    if (str::Eq(mediatype, "application/x-dtbncx+xml")) {
        return true;
    }
    if (str::Eq(mediatype, "text/html")) {
        return true;
    }
    if (str::Eq(mediatype, "text/xml")) {
        return true;
    }
    return false;
}

// TODO: switch to seqstring
static bool isImageMediaType(const char* mediatype) {
    return str::Eq(mediatype, "image/png") || str::Eq(mediatype, "image/jpeg") || str::Eq(mediatype, "image/gif");
}

bool EpubDoc::Load() {
    if (!zip) {
        return false;
    }
    ByteSlice container = zip->GetFileDataByName("META-INF/container.xml");
    if (!container) {
        return false;
    }
    AutoFree contFree = container.Get();
    HtmlParser parser;
    HtmlElement* node = parser.ParseInPlace(container);
    if (!node) {
        return false;
    }

    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByNameNS("rootfile", EPUB_CONTAINER_NS);
    if (!node) {
        return false;
    }
    char* contentPath = node->GetAttributeTemp("full-path");
    if (!contentPath) {
        return false;
    }
    url::DecodeInPlace(contentPath);

    // encrypted files will be ignored (TODO: support decryption)
    StrVec encList;
    ByteSlice encryption = zip->GetFileDataByName("META-INF/encryption.xml");
    if (encryption) {
        (void)parser.ParseInPlace(encryption);
        HtmlElement* cr = parser.FindElementByNameNS("CipherReference", EPUB_ENC_NS);
        while (cr) {
            WCHAR* uriW = cr->GetAttribute("URI");
            if (uriW) {
                char* uri = ToUtf8Temp(uriW);
                url::DecodeInPlace(uri);
                encList.Append(uri);
                str::Free(uriW);
            }
            cr = parser.FindElementByNameNS("CipherReference", EPUB_ENC_NS, cr);
        }
        encryption.Free();
    }

    ByteSlice content = zip->GetFileDataByName(contentPath);
    AutoFree contentFree = content.Get();
    if (!content) {
        return false;
    }
    ParseMetadata(content);
    node = parser.ParseInPlace(content);
    if (!node) {
        return false;
    }
    node = parser.FindElementByNameNS("manifest", EPUB_OPF_NS);
    if (!node) {
        return false;
    }

    char* slashPos = str::FindCharLast(contentPath, '/');
    if (slashPos) {
        *(slashPos + 1) = '\0';
    } else {
        *contentPath = '\0';
    }

    StrVec idList, pathList;

    for (node = node->down; node; node = node->next) {
        char* mediaType = node->GetAttributeTemp("media-type");
        if (isImageMediaType(mediaType)) {
            char* imgPath = node->GetAttributeTemp("href");
            if (!imgPath) {
                continue;
            }
            url::DecodeInPlace(imgPath);
            imgPath = str::JoinTemp(contentPath, imgPath);
            if (encList.Contains(imgPath)) {
                continue;
            }
            // load the image lazily
            ImageData data;
            data.fileName = str::Dup(imgPath);
            data.fileId = zip->GetFileId(data.fileName);
            images.Append(data);
        } else if (isHtmlMediaType(mediaType)) {
            char* htmlPath = node->GetAttributeTemp("href");
            if (!htmlPath) {
                continue;
            }
            url::DecodeInPlace(htmlPath);
            char* htmlId = node->GetAttributeTemp("id");
            // EPUB 3 ToC
            char* properties = node->GetAttributeTemp("properties");
            if (properties && str::Find(properties, "nav") && str::Eq(mediaType, "application/xhtml+xml")) {
                tocPath.Set(str::Join(contentPath, htmlPath));
            }

            char* fullContentPath = str::JoinTemp(contentPath, htmlPath);
            if (encList.Contains(fullContentPath)) {
                continue;
            }
            if (htmlPath && htmlId) {
                idList.Append(htmlId);
                pathList.Append(htmlPath);
            }
        }
    }

    node = parser.FindElementByNameNS("spine", EPUB_OPF_NS);
    if (!node) {
        return false;
    }

    // EPUB 2 ToC
    char* tocId = node->GetAttributeTemp("toc");
    if (tocId && !tocPath && idList.Contains(tocId)) {
        tocPath.Set(str::Join(contentPath, pathList.at(idList.Find(tocId))));
        isNcxToc = true;
    }
    AutoFreeWstr readingDir(node->GetAttribute("page-progression-direction"));
    if (readingDir) {
        isRtlDoc = str::EqI(readingDir, L"rtl");
    }

    for (node = node->down; node; node = node->next) {
        if (!node->NameIsNS("itemref", EPUB_OPF_NS)) {
            continue;
        }
        char* idref = node->GetAttributeTemp("idref");
        if (!idref || !idList.Contains(idref)) {
            continue;
        }

        const char* fname = pathList.at(idList.Find(idref));
        char* fullPath = str::JoinTemp(contentPath, fname);
        ByteSlice html = zip->GetFileDataByName(fullPath);
        if (!html) {
            continue;
        }
        char* decoded = DecodeTextToUtf8(html, true);
        html.Free();
        if (!decoded) {
            continue;
        }
        // insert explicit page-breaks between sections including
        // an anchor with the file name at the top (for internal links)
        ReportIf(str::FindChar(fullPath, '"'));
        str::TransCharsInPlace(fullPath, "\"", "'");
        htmlData.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", fullPath);
        htmlData.Append(decoded);
        str::Free(decoded);
    }

    return htmlData.size() > 0;
}

void EpubDoc::ParseMetadata(const char* content) {
    struct {
        DocumentProperty prop;
        const char* name;
    } metadataMap[] = {
        {DocumentProperty::Title, "dc:title"},         {DocumentProperty::Author, "dc:creator"},
        {DocumentProperty::CreationDate, "dc:date"},   {DocumentProperty::ModificationDate, "dcterms:modified"},
        {DocumentProperty::Subject, "dc:description"}, {DocumentProperty::Copyright, "dc:rights"},
    };

    HtmlPullParser pullParser(content, str::Len(content));
    int insideMetadata = 0;
    HtmlToken* tok;

    while ((tok = pullParser.Next()) != nullptr) {
        if (tok->IsStartTag() && tok->NameIsNS("metadata", EPUB_OPF_NS)) {
            insideMetadata++;
        } else if (tok->IsEndTag() && tok->NameIsNS("metadata", EPUB_OPF_NS)) {
            insideMetadata--;
        }
        if (!insideMetadata) {
            continue;
        }
        if (!tok->IsStartTag()) {
            continue;
        }

        for (int i = 0; i < dimof(metadataMap); i++) {
            // TODO: implement proper namespace support
            if (tok->NameIs(metadataMap[i].name) || Tag_Meta == tok->tag && tok->GetAttrByName("property") &&
                                                        tok->GetAttrByName("property")->ValIs(metadataMap[i].name)) {
                tok = pullParser.Next();
                if (tok && tok->IsText()) {
                    props.Set(metadataMap[i].prop, ResolveHtmlEntities(tok->s, tok->sLen));
                }
                break;
            }
        }
    }
}

ByteSlice EpubDoc::GetHtmlData() const {
    return htmlData.AsByteSlice();
}

ByteSlice* EpubDoc::GetImageData(const char* fileName, const char* pagePath) {
    ScopedCritSec scope(&zipAccess);

    if (!pagePath) {
        CrashIf(true);
        // if we're reparsing, we might not have pagePath, which is needed to
        // build the exact url so try to find a partial match
        // TODO: the correct approach would be to extend reparseIdx into a
        // struct ReparseData, which would include pagePath and all other
        // styling related state (such as nextPageStyle, listDepth, etc. including
        // format specific state such as hiddenDepth and titleCount) and store it
        // in every HtmlPage, but this should work well enough for now
        for (ImageData& img : images) {
            if (str::EndsWithI(img.fileName, fileName)) {
                if (img.base.empty()) {
                    img.base = zip->GetFileDataById(img.fileId);
                }
                if (!img.base.empty()) {
                    return &img.base;
                }
            }
        }
        return nullptr;
    }

    AutoFreeStr url = NormalizeURL(fileName, pagePath);
    // some EPUB producers use wrong path separators
    if (str::FindChar(url, '\\')) {
        str::TransCharsInPlace(url, "\\", "/");
    }
    for (ImageData& img : images) {
        if (str::Eq(img.fileName, url)) {
            if (img.base.empty()) {
                img.base = zip->GetFileDataById(img.fileId);
            }
            if (!img.base.empty()) {
                return &img.base;
            }
        }
    }

    // try to also load images which aren't registered in the manifest
    ImageData data;
    data.fileId = zip->GetFileId(url);
    if (data.fileId != (size_t)-1) {
        data.base = zip->GetFileDataById(data.fileId);
        if (!data.base.empty()) {
            data.fileName = str::Dup(url);
            images.Append(data);
            return &images.Last().base;
        }
    }

    return nullptr;
}

ByteSlice EpubDoc::GetFileData(const char* relPath, const char* pagePath) {
    if (!pagePath) {
        CrashIf(true);
        return {};
    }

    ScopedCritSec scope(&zipAccess);

    AutoFreeStr url = NormalizeURL(relPath, pagePath);
    return zip->GetFileDataByName(url);
}

char* EpubDoc::GetProperty(DocumentProperty prop) const {
    return props.Get(prop);
}

const char* EpubDoc::GetFileName() const {
    return fileName;
}

bool EpubDoc::IsRTL() const {
    return isRtlDoc;
}

bool EpubDoc::HasToc() const {
    return tocPath != nullptr;
}

bool EpubDoc::ParseNavToc(const char* data, size_t dataLen, const char* pagePath, EbookTocVisitor* visitor) {
    HtmlPullParser parser(data, dataLen);
    HtmlToken* tok;
    // skip to the start of the <nav epub:type="toc">
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && Tag_Nav == tok->tag) {
            AttrInfo* attr = tok->GetAttrByName("epub:type");
            if (attr && attr->ValIs("toc")) {
                break;
            }
        }
    }
    if (!tok || tok->IsError()) {
        return false;
    }

    int level = 0;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() && (!tok->IsEndTag() || Tag_Nav != tok->tag)) {
        if (tok->IsStartTag() && Tag_Ol == tok->tag) {
            level++;
        } else if (tok->IsEndTag() && Tag_Ol == tok->tag && level > 0) {
            level--;
        }
        if (tok->IsStartTag() && (Tag_A == tok->tag || Tag_Span == tok->tag)) {
            HtmlTag itemTag = tok->tag;
            AutoFreeStr text, href;
            if (Tag_A == tok->tag) {
                AttrInfo* attrInfo = tok->GetAttrByName("href");
                if (attrInfo) {
                    href.Set(str::Dup(attrInfo->val, attrInfo->valLen));
                }
            }
            while ((tok = parser.Next()) != nullptr && !tok->IsError() && (!tok->IsEndTag() || itemTag != tok->tag)) {
                if (tok->IsText()) {
                    AutoFreeStr part = str::Dup(tok->s, tok->sLen);
                    if (!text) {
                        text.Set(part.Release());
                    } else {
                        text.Set(str::Join(text, part));
                    }
                }
            }
            if (!text) {
                continue;
            }
            auto itemText = ToWStrTemp(text.Get());
            str::NormalizeWSInPlace(itemText);
            AutoFreeWstr itemSrc;
            if (href) {
                href.Set(NormalizeURL(href, pagePath));
                itemSrc.Set(strconv::FromHtmlUtf8(href, str::Len(href)));
            }
            char* txt = ToUtf8Temp(itemText);
            char* src = ToUtf8Temp(itemSrc);
            visitor->Visit(txt, src, level);
        }
    }

    return true;
}

bool EpubDoc::ParseNcxToc(const char* data, size_t dataLen, const char* pagePath, EbookTocVisitor* visitor) {
    HtmlPullParser parser(data, dataLen);
    HtmlToken* tok;
    // skip to the start of the navMap
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && tok->NameIsNS("navMap", EPUB_NCX_NS)) {
            break;
        }
    }
    if (!tok || tok->IsError()) {
        return false;
    }

    AutoFreeWstr itemText, itemSrc;
    int level = 0;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsEndTag() || !tok->NameIsNS("navMap", EPUB_NCX_NS))) {
        if (tok->IsTag() && tok->NameIsNS("navPoint", EPUB_NCX_NS)) {
            if (itemText) {
                char* txt = ToUtf8Temp(itemText);
                char* src = ToUtf8Temp(itemSrc);
                visitor->Visit(txt, src, level);
                itemText.Reset();
                itemSrc.Reset();
            }
            if (tok->IsStartTag()) {
                level++;
            } else if (tok->IsEndTag() && level > 0) {
                level--;
            }
        } else if (tok->IsStartTag() && tok->NameIsNS("text", EPUB_NCX_NS)) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                itemText.Set(strconv::FromHtmlUtf8(tok->s, tok->sLen));
            }
        } else if (tok->IsTag() && !tok->IsEndTag() && tok->NameIsNS("content", EPUB_NCX_NS)) {
            AttrInfo* attrInfo = tok->GetAttrByName("src");
            if (attrInfo) {
                AutoFreeStr src = str::Dup(attrInfo->val, attrInfo->valLen);
                src.Set(NormalizeURL(src, pagePath));
                itemSrc.Set(strconv::FromHtmlUtf8(src, str::Len(src)));
            }
        }
    }

    return true;
}

bool EpubDoc::ParseToc(EbookTocVisitor* visitor) {
    if (!tocPath) {
        return false;
    }
    size_t tocDataLen;
    AutoFree tocData;
    {
        ScopedCritSec scope(&zipAccess);
        ByteSlice res = zip->GetFileDataByName(tocPath);
        tocDataLen = res.size();
        tocData.Set(res);
    }
    if (!tocData) {
        return false;
    }

    char* pagePath = tocPath.Get();
    if (isNcxToc) {
        return ParseNcxToc(tocData, tocDataLen, pagePath, visitor);
    }
    return ParseNavToc(tocData, tocDataLen, pagePath, visitor);
}

bool EpubDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFileEpub;
}

EpubDoc* EpubDoc::CreateFromFile(const char* path) {
    EpubDoc* doc = new EpubDoc(path);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

EpubDoc* EpubDoc::CreateFromStream(IStream* stream) {
    EpubDoc* doc = new EpubDoc(stream);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

/* ********** FictionBook (FB2) ********** */

const char* FB2_MAIN_NS = "http://www.gribuser.ru/xml/fictionbook/2.0";
const char* FB2_XLINK_NS = "http://www.w3.org/1999/xlink";

Fb2Doc::Fb2Doc(const char* fileName) : fileName(str::Dup(fileName)) {
}

Fb2Doc::Fb2Doc(IStream* stream) : stream(stream) {
    stream->AddRef();
}

Fb2Doc::~Fb2Doc() {
    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName);
    }
    if (stream) {
        stream->Release();
    }
}

static ByteSlice loadFromFile(Fb2Doc* doc) {
    MultiFormatArchive* archive = OpenZipArchive(doc->fileName, false);
    if (!archive) {
        return file::ReadFile(doc->fileName);
    }

    AutoDelete delArchive(archive);

    // we have archive with more than 1 file
    doc->isZipped = true;
    auto& fileInfos = archive->GetFileInfos();
    size_t nFiles = fileInfos.size();

    if (nFiles == 0) {
        return {};
    }

    if (nFiles == 1) {
        return archive->GetFileDataById(0);
    }

    ByteSlice data;
    // if the ZIP file contains more than one file, we try to be rather
    // restrictive in what we accept in order not to accidentally accept
    // too many archives which only contain FB2 files among others:
    // the file must contain a single .fb2 file and may only contain
    // .url files in addition (TODO: anything else?)
    for (auto&& fileInfo : fileInfos) {
        auto path = fileInfo->name;
        if (str::EndsWithI(path, ".fb2") && data.empty()) {
            data = archive->GetFileDataById(fileInfo->fileId);
        } else if (!str::EndsWithI(path, ".url")) {
            return {};
        }
    }
    return data;
}

static ByteSlice loadFromStream(Fb2Doc* doc) {
    auto stream = doc->stream;
    MultiFormatArchive* archive = OpenZipArchive(stream, false);
    if (!archive) {
        return {};
    }

    AutoDelete delArchive(archive);
    size_t nFiles = archive->GetFileInfos().size();
    if (nFiles != 1) {
        return {};
    }
    doc->isZipped = true;
    return archive->GetFileDataById(0);
}

bool Fb2Doc::Load() {
    CrashIf(!stream && !fileName);

    ByteSlice data;
    if (fileName) {
        data = loadFromFile(this);
    } else if (stream) {
        data = loadFromStream(this);
    }
    if (data.empty()) {
        return false;
    }
    AutoFreeStr tmp = DecodeTextToUtf8(data, true);
    data.Free();
    if (!tmp) {
        return false;
    }

    ByteSlice data2(tmp.Get());

    HtmlPullParser parser(data2);
    HtmlToken* tok;
    int inBody = 0, inTitleInfo = 0, inDocInfo = 0;
    const char* bodyStart = nullptr;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!inTitleInfo && !inDocInfo && tok->IsStartTag() && Tag_Body == tok->tag) {
            if (!inBody++) {
                bodyStart = tok->s;
            }
        } else if (inBody && tok->IsEndTag() && Tag_Body == tok->tag) {
            if (!--inBody) {
                if (xmlData.size() > 0) {
                    xmlData.Append("<pagebreak />");
                }
                xmlData.AppendChar('<');
                xmlData.Append(bodyStart, tok->s - bodyStart + tok->sLen);
                xmlData.AppendChar('>');
            }
        } else if (inBody && tok->IsStartTag() && Tag_Title == tok->tag) {
            hasToc = true;
        } else if (inBody) {
            continue;
        } else if (inTitleInfo && tok->IsEndTag() && tok->NameIsNS("title-info", FB2_MAIN_NS)) {
            inTitleInfo--;
        } else if (inDocInfo && tok->IsEndTag() && tok->NameIsNS("document-info", FB2_MAIN_NS)) {
            inDocInfo--;
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("book-title", FB2_MAIN_NS)) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                props.Set(DocumentProperty::Title, ResolveHtmlEntities(tok->s, tok->sLen));
            }
        } else if ((inTitleInfo || inDocInfo) && tok->IsStartTag() && tok->NameIsNS("author", FB2_MAIN_NS)) {
            AutoFreeStr docAuthor;
            while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
                   !(tok->IsEndTag() && tok->NameIsNS("author", FB2_MAIN_NS))) {
                if (tok->IsText()) {
                    AutoFreeStr author = ResolveHtmlEntities(tok->s, tok->sLen);
                    if (docAuthor) {
                        docAuthor.Set(str::Join(docAuthor, " ", author));
                    } else {
                        docAuthor.Set(author.Release());
                    }
                }
            }
            if (docAuthor) {
                str::NormalizeWSInPlace(docAuthor);
                if (!str::IsEmpty(docAuthor.Get())) {
                    props.Set(DocumentProperty::Author, docAuthor.Release(), inTitleInfo != 0);
                }
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("date", FB2_MAIN_NS)) {
            AttrInfo* attr = tok->GetAttrByNameNS("value", FB2_MAIN_NS);
            if (attr) {
                props.Set(DocumentProperty::CreationDate, ResolveHtmlEntities(attr->val, attr->valLen));
            }
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS("date", FB2_MAIN_NS)) {
            AttrInfo* attr = tok->GetAttrByNameNS("value", FB2_MAIN_NS);
            if (attr) {
                props.Set(DocumentProperty::ModificationDate, ResolveHtmlEntities(attr->val, attr->valLen));
            }
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS("program-used", FB2_MAIN_NS)) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                props.Set(DocumentProperty::CreatorApp, ResolveHtmlEntities(tok->s, tok->sLen));
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("coverpage", FB2_MAIN_NS)) {
            tok = parser.Next();
            if (tok && tok->IsText()) {
                tok = parser.Next();
            }
            if (tok && tok->IsEmptyElementEndTag() && Tag_Image == tok->tag) {
                AttrInfo* attr = tok->GetAttrByNameNS("href", FB2_XLINK_NS);
                if (attr) {
                    coverImage.Set(str::Dup(attr->val, attr->valLen));
                }
            }
        } else if (inTitleInfo || inDocInfo) {
            continue;
        } else if (tok->IsStartTag() && tok->NameIsNS("title-info", FB2_MAIN_NS)) {
            inTitleInfo++;
        } else if (tok->IsStartTag() && tok->NameIsNS("document-info", FB2_MAIN_NS)) {
            inDocInfo++;
        } else if (tok->IsStartTag() && tok->NameIsNS("binary", FB2_MAIN_NS)) {
            ExtractImage(&parser, tok);
        }
    }

    return xmlData.size() > 0;
}

void Fb2Doc::ExtractImage(HtmlPullParser* parser, HtmlToken* tok) {
    AutoFreeStr id;
    AttrInfo* attrInfo = tok->GetAttrByNameNS("id", FB2_MAIN_NS);
    if (attrInfo) {
        id.Set(str::Dup(attrInfo->val, attrInfo->valLen));
        url::DecodeInPlace(id);
    }

    tok = parser->Next();
    if (!tok || !tok->IsText()) {
        return;
    }

    ImageData data;
    data.base = Base64Decode({(u8*)tok->s, tok->sLen});
    if (data.base.empty()) {
        return;
    }
    data.fileName = str::Join("#", id);
    data.fileId = images.size();
    images.Append(data);
}

ByteSlice Fb2Doc::GetXmlData() const {
    return {(u8*)xmlData.Get(), xmlData.size()};
}

ByteSlice* Fb2Doc::GetImageData(const char* fileName) const {
    for (size_t i = 0; i < images.size(); i++) {
        if (str::Eq(images.at(i).fileName, fileName)) {
            return &images.at(i).base;
        }
    }
    return nullptr;
}

ByteSlice* Fb2Doc::GetCoverImage() const {
    if (!coverImage) {
        return nullptr;
    }
    return GetImageData(coverImage);
}

char* Fb2Doc::GetProperty(DocumentProperty prop) const {
    return props.Get(prop);
}

const char* Fb2Doc::GetFileName() const {
    return fileName;
}

bool Fb2Doc::IsZipped() const {
    return isZipped;
}

bool Fb2Doc::HasToc() const {
    return hasToc;
}

bool Fb2Doc::ParseToc(EbookTocVisitor* visitor) const {
    AutoFreeWstr itemText;
    bool inTitle = false;
    int titleCount = 0;
    int level = 0;

    auto xmlData2 = GetXmlData();
    HtmlPullParser parser(xmlData2);
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && Tag_Section == tok->tag) {
            level++;
        } else if (tok->IsEndTag() && Tag_Section == tok->tag && level > 0) {
            level--;
        } else if (tok->IsStartTag() && Tag_Title == tok->tag) {
            inTitle = true;
            titleCount++;
        } else if (tok->IsEndTag() && Tag_Title == tok->tag) {
            if (itemText) {
                str::NormalizeWSInPlace(itemText);
            }
            if (!str::IsEmpty(itemText.Get())) {
                TempStr url = str::FormatTemp(FB2_TOC_ENTRY_MARK "%d", titleCount);
                char* txt = ToUtf8Temp(itemText);
                visitor->Visit(txt, url, level);
                itemText.Reset();
            }
            inTitle = false;
        } else if (inTitle && tok->IsText()) {
            AutoFreeWstr text(strconv::FromHtmlUtf8(tok->s, tok->sLen));
            if (str::IsEmpty(itemText.Get())) {
                itemText.Set(text.StealData());
            } else {
                itemText.Set(str::Join(itemText, L" ", text));
            }
        }
    }

    return true;
}

bool Fb2Doc::IsSupportedFileType(Kind kind) {
    return kind == kindFileFb2 || kind == kindFileFb2z;
}

Fb2Doc* Fb2Doc::CreateFromFile(const char* path) {
    Fb2Doc* doc = new Fb2Doc(path);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

Fb2Doc* Fb2Doc::CreateFromStream(IStream* stream) {
    Fb2Doc* doc = new Fb2Doc(stream);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

/* ********** PalmDOC (and TealDoc) ********** */

PalmDoc::PalmDoc(const char* path) {
    this->fileName = str::Dup(path);
}

PalmDoc::~PalmDoc() {
}

#define PDB_TOC_ENTRY_MARK "ToC!Entry!"

// http://wiki.mobileread.com/wiki/TealDoc
static const char* HandleTealDocTag(str::Str& builder, StrVec& tocEntries, const char* text, size_t len, uint) {
    if (len < 9) {
    Fallback:
        builder.Append("&lt;");
        return text;
    }
    if (!str::StartsWithI(text, "<BOOKMARK") && !str::StartsWithI(text, "<HEADER") &&
        !str::StartsWithI(text, "<HRULE") && !str::StartsWithI(text, "<LABEL") && !str::StartsWithI(text, "<LINK") &&
        !str::StartsWithI(text, "<TEALPAINT")) {
        goto Fallback;
    }
    HtmlPullParser parser(text, len);
    HtmlToken* tok = parser.Next();
    if (!tok || !tok->IsStartTag()) {
        goto Fallback;
    }

    if (tok->NameIs("BOOKMARK")) {
        // <BOOKMARK NAME="Contents">
        AttrInfo* attr = tok->GetAttrByName("NAME");
        if (attr && attr->valLen > 0) {
            WCHAR* ws = strconv::FromHtmlUtf8(attr->val, attr->valLen);
            char* s = ToUtf8Temp(ws);
            tocEntries.Append(s);
            str::Free(ws);
            builder.AppendFmt("<a name=" PDB_TOC_ENTRY_MARK "%d>", (int)tocEntries.size());
            return tok->s + tok->sLen;
        }
    } else if (tok->NameIs("HEADER")) {
        // <HEADER TEXT="Contents" ALIGN=CENTER STYLE=UNDERLINE>
        int hx = 2;
        AttrInfo* attr = tok->GetAttrByName("FONT");
        if (attr && attr->valLen > 0) {
            hx = '0' == *attr->val ? 5 : '2' == *attr->val ? 1 : 3;
        }
        attr = tok->GetAttrByName("TEXT");
        if (attr) {
            builder.AppendFmt("<h%d>", hx);
            builder.Append(attr->val, attr->valLen);
            builder.AppendFmt("</h%d>", hx);
            return tok->s + tok->sLen;
        }
    } else if (tok->NameIs("HRULE")) {
        // <HRULE STYLE=OUTLINE>
        builder.Append("<hr>");
        return tok->s + tok->sLen;
    } else if (tok->NameIs("LABEL")) {
        // <LABEL NAME="Contents">
        AttrInfo* attr = tok->GetAttrByName("NAME");
        if (attr && attr->valLen > 0) {
            builder.Append("<a name=\"");
            builder.Append(attr->val, attr->valLen);
            builder.Append("\">");
            return tok->s + tok->sLen;
        }
    } else if (tok->NameIs("LINK")) {
        // <LINK TEXT="Press Me" TAG="Contents" FILE="My Novels">
        AttrInfo* attrTag = tok->GetAttrByName("TAG");
        AttrInfo* attrText = tok->GetAttrByName("TEXT");
        if (attrTag && attrText) {
            if (tok->GetAttrByName("FILE")) {
                // skip links to other files
                return tok->s + tok->sLen;
            }
            builder.Append("<a href=\"#");
            builder.Append(attrTag->val, attrTag->valLen);
            builder.Append("\">");
            builder.Append(attrText->val, attrText->valLen);
            builder.Append("</a>");
            return tok->s + tok->sLen;
        }
    } else if (tok->NameIs("TEALPAINT")) {
        // <TEALPAINT SRC="Pictures" INDEX=0 LINK=SUPERMAP SUPERIMAGE=1 SUPERW=640 SUPERH=480>
        // support removed in r7047
        return tok->s + tok->sLen;
    }
    goto Fallback;
}

bool PalmDoc::Load() {
    MobiDoc* mobiDoc = MobiDoc::CreateFromFile(fileName);
    if (!mobiDoc) {
        return false;
    }
    auto docType = mobiDoc->GetDocType();
    switch (docType) {
        case PdbDocType::PalmDoc:
        case PdbDocType::TealDoc:
        case PdbDocType::Plucker:
            // no-op
            break;
        default:
            delete mobiDoc;
            return false;
    }

    ByteSlice text = mobiDoc->GetHtmlData();
    uint codePage = GuessTextCodepage((const char*)text.data(), text.size(), CP_ACP);
    char* textUtf8 = strconv::ToMultiByte((const char*)text.data(), codePage, CP_UTF8);

    const char* start = textUtf8;
    const char* end = start + str::Len(textUtf8);
    // TODO: speedup by not calling htmlData.Append() for every byte
    // but gather spans and memcpy them wholesale
    for (const char* curr = start; curr < end; curr++) {
        char c = *curr;
        if ('&' == c) {
            htmlData.Append("&amp;");
        } else if ('<' == c) {
            curr = HandleTealDocTag(htmlData, tocEntries, curr, end - curr, codePage);
        } else if ('\n' == c || '\r' == c && curr + 1 < end && '\n' != *(curr + 1)) {
            htmlData.Append("\n<br>");
        } else {
            htmlData.AppendChar(c);
        }
    }

    delete mobiDoc;
    str::Free(textUtf8);
    return true;
}

ByteSlice PalmDoc::GetHtmlData() const {
    return htmlData.AsByteSlice();
}

char* PalmDoc::GetProperty(DocumentProperty) const {
    return nullptr;
}

const char* PalmDoc::GetFileName() const {
    return fileName;
}

bool PalmDoc::HasToc() const {
    return tocEntries.size() > 0;
}

bool PalmDoc::ParseToc(EbookTocVisitor* visitor) {
    for (int i = 0; i < tocEntries.Size(); i++) {
        TempStr url = str::FormatTemp(PDB_TOC_ENTRY_MARK "%d", i + 1);
        char* name = tocEntries[i];
        visitor->Visit(name, url, 1);
    }
    return true;
}

bool PalmDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFilePalmDoc;
}

PalmDoc* PalmDoc::CreateFromFile(const char* path) {
    PalmDoc* doc = new PalmDoc(path);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

/* ********** Plain HTML ********** */

HtmlDoc::HtmlDoc(const char* path) : fileName(str::Dup(path)) {
}

HtmlDoc::~HtmlDoc() {
    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName);
    }
    htmlData.Free();
}

bool HtmlDoc::Load() {
    {
        ByteSlice data = file::ReadFile(fileName);
        if (!data) {
            return false;
        }
        char* decoded = DecodeTextToUtf8(data, true);
        if (!decoded) {
            return false;
        }
        htmlData = decoded;
        data.Free();
    }

    pagePath.SetCopy(fileName);
    str::TransCharsInPlace(pagePath, "\\", "/");

    HtmlPullParser parser(htmlData);
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsTag() || Tag_Body != tok->tag && Tag_P != tok->tag)) {
        if (tok->IsStartTag() && Tag_Title == tok->tag) {
            tok = parser.Next();
            if (tok && tok->IsText()) {
                props.Set(DocumentProperty::Title, ResolveHtmlEntities(tok->s, tok->sLen));
            }
        } else if ((tok->IsStartTag() || tok->IsEmptyElementEndTag()) && Tag_Meta == tok->tag) {
            AttrInfo* attrName = tok->GetAttrByName("name");
            AttrInfo* attrValue = tok->GetAttrByName("content");
            if (!attrName || !attrValue) {
                /* ignore this tag */;
            } else if (attrName->ValIs("author")) {
                props.Set(DocumentProperty::Author, ResolveHtmlEntities(attrValue->val, attrValue->valLen));
            } else if (attrName->ValIs("date")) {
                props.Set(DocumentProperty::CreationDate, ResolveHtmlEntities(attrValue->val, attrValue->valLen));
            } else if (attrName->ValIs("copyright")) {
                props.Set(DocumentProperty::Copyright, ResolveHtmlEntities(attrValue->val, attrValue->valLen));
            }
        }
    }

    return true;
}

ByteSlice HtmlDoc::GetHtmlData() {
    return htmlData;
}

ByteSlice* HtmlDoc::GetImageData(const char* fileName) {
    // TODO: this isn't thread-safe (might leak image data when called concurrently),

    AutoFreeStr url = NormalizeURL(fileName, pagePath);
    for (size_t i = 0; i < images.size(); i++) {
        if (str::Eq(images.at(i).fileName, url)) {
            return &images.at(i).base;
        }
    }

    ImageData data;
    data.base = LoadURL(url);
    if (data.base.empty()) {
        return nullptr;
    }
    data.fileName = url.Release();
    images.Append(data);
    return &images.Last().base;
}

ByteSlice HtmlDoc::GetFileData(const char* relPath) {
    AutoFreeStr url = NormalizeURL(relPath, pagePath);
    return LoadURL(url);
}

ByteSlice HtmlDoc::LoadURL(const char* url) {
    if (str::StartsWith(url, "data:")) {
        auto res = DecodeDataURI(url);
        return res;
    }
    if (str::FindChar(url, ':')) {
        return {};
    }
    char* path = str::Dup(url);
    str::TransCharsInPlace(path, "/", "\\");
    return file::ReadFile(path);
}

char* HtmlDoc::GetProperty(DocumentProperty prop) const {
    return props.Get(prop);
}

const char* HtmlDoc::GetFileName() const {
    return fileName;
}

bool HtmlDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFilePalmDoc;
}

HtmlDoc* HtmlDoc::CreateFromFile(const char* fileName) {
    HtmlDoc* doc = new HtmlDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

/* ********** Plain Text (and RFCs and TCR) ********** */

TxtDoc::TxtDoc(const char* fileName) {
    this->fileName = str::Dup(fileName);
}

// cf. http://www.cix.co.uk/~gidds/Software/TCR.html
#define TCR_HEADER "!!8-Bit!!"

static char* DecompressTcrText(const char* data, size_t dataLen) {
    CrashIf(!str::StartsWith(data, TCR_HEADER));
    const char* curr = data + str::Len(TCR_HEADER);
    const char* end = data + dataLen;

    const char* dict[256] = {0};
    for (int n = 0; n < (int)dimof(dict); n++) {
        if (curr >= end) {
            return str::Dup(data);
        }
        dict[n] = curr;
        curr += 1 + (u8)*curr;
    }

    str::Str text(dataLen * 2);
    InterlockedIncrement(&gAllowAllocFailure);
    defer {
        InterlockedDecrement(&gAllowAllocFailure);
    };

    for (; curr < end; curr++) {
        const char* entry = dict[(u8)*curr];
        bool ok = text.Append(entry + 1, (u8)*entry);
        if (!ok) {
            return nullptr;
        }
    }

    return text.StealData();
}

static const char* TextFindLinkEnd(str::Str& htmlData, const char* curr, char prevChar, bool fromWww = false) {
    const char *end, *quote;

    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    for (end = curr; *end && !str::IsWs(*end); end++) {
        ;
    }
    if (',' == end[-1] || '.' == end[-1] || '?' == end[-1] || '!' == end[-1]) {
        end--;
    }
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == end[-1] && (!str::FindChar(curr, '(') || str::FindChar(curr, '(') >= end)) {
        end--;
    }
    // cut the link at the first quotation mark, if it's also preceded by one
    if (('"' == prevChar || '\'' == prevChar) && (quote = str::FindChar(curr, prevChar)) != nullptr && quote < end) {
        end = quote;
    }

    if (fromWww && (end - curr <= 4 || !str::FindChar(curr + 5, '.') || str::FindChar(curr + 5, '.') >= end)) {
        // ignore www. links without a top-level domain
        return nullptr;
    }

    htmlData.Append("<a href=\"");
    if (fromWww) {
        htmlData.Append("http://");
    }
    for (; curr < end; curr++) {
        AppendChar(htmlData, *curr);
    }
    htmlData.Append("\">");

    return end;
}

// cf. http://weblogs.mozillazine.org/gerv/archives/2011/05/html5_email_address_regexp.html
inline bool IsEmailUsernameChar(char c) {
    // explicitly excluding the '/' from the list, as it is more
    // often part of a URL or path than of an email address
    return isalnum((u8)c) || c && str::FindChar(".!#$%&'*+=?^_`{|}~-", c);
}
inline bool IsEmailDomainChar(char c) {
    return isalnum((u8)c) || '-' == c;
}

static const char* TextFindEmailEnd(str::Str& htmlData, const char* curr) {
    AutoFreeStr beforeAt;
    const char* end = curr;
    if ('@' == *curr) {
        if (htmlData.size() == 0 || !IsEmailUsernameChar(htmlData.Last())) {
            return nullptr;
        }
        size_t idx = htmlData.size();
        for (; idx > 1 && IsEmailUsernameChar(htmlData.at(idx - 1)); idx--) {
            ;
        }
        beforeAt.SetCopy(&htmlData.at(idx));
    } else {
        CrashIf(!str::StartsWith(curr, "mailto:"));
        end = curr = curr + 7; // skip mailto:
        if (!IsEmailUsernameChar(*end)) {
            return nullptr;
        }
        for (; IsEmailUsernameChar(*end); end++) {
            ;
        }
    }

    if (*end != '@' || !IsEmailDomainChar(*(end + 1))) {
        return nullptr;
    }
    for (end++; IsEmailDomainChar(*end); end++) {
        ;
    }
    if ('.' != *end || !IsEmailDomainChar(*(end + 1))) {
        return nullptr;
    }
    do {
        for (end++; IsEmailDomainChar(*end); end++) {
            ;
        }
    } while ('.' == *end && IsEmailDomainChar(*(end + 1)));

    if (beforeAt) {
        size_t idx = htmlData.size() - str::Len(beforeAt);
        htmlData.RemoveAt(idx, htmlData.size() - idx);
    }
    htmlData.Append("<a href=\"mailto:");
    htmlData.Append(beforeAt);
    for (; curr < end; curr++) {
        AppendChar(htmlData, *curr);
    }
    htmlData.Append("\">");
    htmlData.Append(beforeAt);

    return end;
}

static const char* TextFindRfcEnd(str::Str& htmlData, const char* curr) {
    if (isalnum((u8) * (curr - 1))) {
        return nullptr;
    }
    int rfc;
    const char* end = str::Parse(curr, "RFC %d", &rfc);
    // cf. http://en.wikipedia.org/wiki/Request_for_Comments#Obtaining_RFCs
    htmlData.AppendFmt("<a href='http://www.rfc-editor.org/rfc/rfc%d.txt'>", rfc);
    return end;
}

bool TxtDoc::Load() {
    ByteSlice fileContent = file::ReadFile(fileName);
    if (!fileContent) {
        return false;
    }

    AutoFreeStr text(fileContent);
    if (str::EndsWithI(fileName, ".tcr") && str::StartsWith(text.data, TCR_HEADER)) {
        char* s = DecompressTcrText(fileContent, fileContent.size());
        if (!s) {
            return false;
        }
        text.TakeOwnershipOf(s);
    }
    char* s = DecodeTextToUtf8(text.data);
    if (!s) {
        return false;
    }
    text.TakeOwnershipOf(s);

    int rfc;
    isRFC = str::Parse(path::GetBaseNameTemp(fileName), "rfc%d.txt%$", &rfc) != nullptr;

    const char* linkEnd = nullptr;
    bool rfcHeader = false;
    int sectionCount = 0;

    htmlData.Append("<pre>");
    char* d = text.data;
    for (const char* curr = d; *curr; curr++) {
        // similar logic to LinkifyText in PdfEngine.cpp
        if (linkEnd == curr) {
            htmlData.Append("</a>");
            linkEnd = nullptr;
        } else if (linkEnd) {
            /* don't check for hyperlinks inside a link */;
        } else if ('@' == *curr) {
            linkEnd = TextFindEmailEnd(htmlData, curr);
        } else if (curr > d && ('/' == curr[-1] || isalnum((u8)curr[-1]))) {
            /* don't check for a link at this position */;
        } else if ('h' == *curr && str::Parse(curr, "http%?s://")) {
            linkEnd = TextFindLinkEnd(htmlData, curr, curr > d ? curr[-1] : ' ');
        } else if ('w' == *curr && str::StartsWith(curr, "www.")) {
            linkEnd = TextFindLinkEnd(htmlData, curr, curr > d ? curr[-1] : ' ', true);
        } else if ('m' == *curr && str::StartsWith(curr, "mailto:")) {
            linkEnd = TextFindEmailEnd(htmlData, curr);
        } else if (isRFC && curr > d && 'R' == *curr && str::Parse(curr, "RFC %d", &rfc)) {
            linkEnd = TextFindRfcEnd(htmlData, curr);
        }

        // RFCs use (among others) form feeds as page separators
        if ('\f' == *curr && (curr == d || '\n' == *(curr - 1)) &&
            (!*(curr + 1) || '\r' == *(curr + 1) || '\n' == *(curr + 1))) {
            // only insert pagebreaks if not at the very beginning or end
            if (curr > d && *(curr + 2) && (*(curr + 3) || *(curr + 2) != '\n')) {
                htmlData.Append("<pagebreak />");
            }
            continue;
        }

        if (isRFC && curr > d && '\n' == *(curr - 1) && (str::IsDigit(*curr) || str::StartsWith(curr, "APPENDIX")) &&
            str::FindChar(curr, '\n') && str::Parse(str::FindChar(curr, '\n') + 1, "%?\r\n")) {
            htmlData.AppendFmt("<b id='section%d' title=\"", ++sectionCount);
            for (const char* c = curr; *c != '\r' && *c != '\n'; c++) {
                AppendChar(htmlData, *c);
            }
            htmlData.Append("\">");
            rfcHeader = true;
        }
        if (rfcHeader && ('\r' == *curr || '\n' == *curr)) {
            htmlData.Append("</b>");
            rfcHeader = false;
        }

        AppendChar(htmlData, *curr);
    }
    if (linkEnd) {
        htmlData.Append("</a>");
    }
    htmlData.Append("</pre>");

    return true;
}

ByteSlice TxtDoc::GetHtmlData() const {
    return htmlData.AsByteSlice();
}

char* TxtDoc::GetProperty(DocumentProperty) const {
    return nullptr;
}

const char* TxtDoc::GetFileName() const {
    return fileName;
}

bool TxtDoc::IsRFC() const {
    return isRFC;
}

bool TxtDoc::HasToc() const {
    return isRFC;
}

static inline const WCHAR* SkipDigits(const WCHAR* s) {
    while (str::IsDigit(*s)) {
        s++;
    }
    return s;
}

bool TxtDoc::ParseToc(EbookTocVisitor* visitor) {
    if (!isRFC) {
        return false;
    }

    HtmlParser parser;
    parser.Parse(htmlData.AsByteSlice(), CP_UTF8);
    HtmlElement* el = nullptr;
    while ((el = parser.FindElementByName("b", el)) != nullptr) {
        AutoFreeWstr title(el->GetAttribute("title"));
        AutoFreeWstr id(el->GetAttribute("id"));
        int level = 1;
        if (str::IsDigit(*title)) {
            const WCHAR* dot = SkipDigits(title);
            while ('.' == *dot && str::IsDigit(*(dot + 1))) {
                level++;
                dot = SkipDigits(dot + 1);
            }
        }
        char* titleA = ToUtf8Temp(title);
        char* idA = ToUtf8Temp(id);
        visitor->Visit(titleA, idA, level);
    }

    return true;
}

bool TxtDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFileTxt;
}

TxtDoc* TxtDoc::CreateFromFile(const char* fileName) {
    TxtDoc* doc = new TxtDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}
