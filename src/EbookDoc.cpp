/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/PalmDbReader.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"

// tries to extract an encoding from <?xml encoding="..."?>
// returns CP_ACP on failure
static UINT GetCodepageFromPI(const char* xmlPI) {
    if (!str::StartsWith(xmlPI, "<?xml"))
        return CP_ACP;
    const char* xmlPIEnd = str::Find(xmlPI, "?>");
    if (!xmlPIEnd)
        return CP_ACP;
    HtmlToken pi;
    pi.SetTag(HtmlToken::EmptyElementTag, xmlPI + 2, xmlPIEnd);
    pi.nLen = 4;
    AttrInfo* enc = pi.GetAttrByName("encoding");
    if (!enc)
        return CP_ACP;

    AutoFree encoding(str::DupN(enc->val, enc->valLen));
    struct {
        const char* namePart;
        UINT codePage;
    } encodings[] = {
        {"UTF", CP_UTF8}, {"utf", CP_UTF8}, {"1252", 1252}, {"1251", 1251},
        // TODO: any other commonly used codepages?
    };
    for (size_t i = 0; i < dimof(encodings); i++) {
        if (str::Find(encoding, encodings[i].namePart))
            return encodings[i].codePage;
    }
    return CP_ACP;
}

static bool IsValidUtf8(const char* string) {
    for (const unsigned char* s = (const unsigned char*)string; *s; s++) {
        int skip;
        if (*s < 0x80)
            skip = 0;
        else if (*s < 0xC0)
            return false;
        else if (*s < 0xE0)
            skip = 1;
        else if (*s < 0xF0)
            skip = 2;
        else if (*s < 0xF5)
            skip = 3;
        else
            return false;
        while (skip-- > 0) {
            if ((*++s & 0xC0) != 0x80)
                return false;
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
        auto tmp2 = strconv::WstrToUtf8((WCHAR*)(s + 2));
        return (char*)tmp2.data();
    }
    if (str::StartsWith(s, UTF8_BOM))
        return str::Dup(s + 3);
    UINT codePage = isXML ? GetCodepageFromPI(s) : CP_ACP;
    if (CP_ACP == codePage && IsValidUtf8(s))
        return str::Dup(s);
    if (CP_ACP == codePage)
        codePage = GuessTextCodepage(s, str::Len(s), CP_ACP);
    auto tmp2 = strconv::ToMultiByte(s, codePage, CP_UTF8);
    return (char*)tmp2.data();
}

char* NormalizeURL(const char* url, const char* base) {
    CrashIf(!url || !base);
    if (*url == '/' || str::FindChar(url, ':'))
        return str::Dup(url);

    const char* baseEnd = str::FindCharLast(base, '/');
    const char* hash = str::FindChar(base, '#');
    if (*url == '#') {
        baseEnd = hash ? hash - 1 : base + str::Len(base) - 1;
    } else if (baseEnd && hash && hash < baseEnd) {
        for (baseEnd = hash - 1; baseEnd > base && *baseEnd != '/'; baseEnd--)
            ;
    }
    if (baseEnd)
        baseEnd++;
    else
        baseEnd = base;
    AutoFree basePath(str::DupN(base, baseEnd - base));
    AutoFree norm(str::Join(basePath, url));

    char* dst = norm;
    for (char* src = norm; *src; src++) {
        if (*src != '/')
            *dst++ = *src;
        else if (str::StartsWith(src, "/./"))
            src++;
        else if (str::StartsWith(src, "/../")) {
            for (; dst > norm && *(dst - 1) != '/'; dst--)
                ;
            src += 3;
        } else
            *dst++ = '/';
    }
    *dst = '\0';
    return norm.release();
}

inline char decode64(char c) {
    if ('A' <= c && c <= 'Z')
        return c - 'A';
    if ('a' <= c && c <= 'z')
        return c - 'a' + 26;
    if ('0' <= c && c <= '9')
        return c - '0' + 52;
    if ('+' == c)
        return 62;
    if ('/' == c)
        return 63;
    return -1;
}

static char* Base64Decode(const char* s, size_t sLen, size_t* lenOut) {
    const char* end = s + sLen;
    char* result = AllocArray<char>(sLen * 3 / 4);
    char* curr = result;
    unsigned char c = 0;
    int step = 0;
    for (; s < end && *s != '='; s++) {
        char n = decode64(*s);
        if (-1 == n) {
            if (str::IsWs(*s))
                continue;
            free(result);
            return nullptr;
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
    if (lenOut)
        *lenOut = curr - result;
    return result;
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

static std::string_view DecodeDataURI(const char* url) {
    const char* comma = str::FindChar(url, ',');
    if (!comma) {
        return {};
    }
    const char* data = comma + 1;
    if (comma - url >= 12 && str::EqN(comma - 7, ";base64", 7)) {
        size_t lenOut = 0;
        char* d = Base64Decode(data, str::Len(data), &lenOut);
        return {d, lenOut};
    }
    return str::Dup(data);
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
    if (-1 == idx || !replace && values[idx])
        free(valueUtf8);
    else
        values[idx].Set(valueUtf8);
}

WCHAR* PropertyMap::Get(DocumentProperty prop) const {
    int idx = Find(prop);
    if (idx >= 0 && values[idx]) {
        return strconv::Utf8ToWstr(values[idx].get());
    }
    return nullptr;
}

/* ********** EPUB ********** */

const char* EPUB_CONTAINER_NS = "urn:oasis:names:tc:opendocument:xmlns:container";
const char* EPUB_OPF_NS = "http://www.idpf.org/2007/opf";
const char* EPUB_NCX_NS = "http://www.daisy.org/z3986/2005/ncx/";
const char* EPUB_ENC_NS = "http://www.w3.org/2001/04/xmlenc#";

EpubDoc::EpubDoc(const WCHAR* fileName) {
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

    for (size_t i = 0; i < images.size(); i++) {
        free(images.at(i).base.data);
        free(images.at(i).fileName);
    }

    LeaveCriticalSection(&zipAccess);
    DeleteCriticalSection(&zipAccess);
    delete zip;
}

// TODO: switch to seqstring
static bool isHtmlMediaType(const WCHAR* mediatype) {
    if (str::Eq(mediatype, L"application/xhtml+xml")) {
        return true;
    }
    if (str::Eq(mediatype, L"application/html+xml")) {
        return true;
    }
    if (str::Eq(mediatype, L"application/x-dtbncx+xml")) {
        return true;
    }
    if (str::Eq(mediatype, L"text/html")) {
        return true;
    }
    if (str::Eq(mediatype, L"text/xml")) {
        return true;
    }
    return false;
}

// TODO: switch to seqstring
static bool isImageMediaType(const WCHAR* mediatype) {
    return str::Eq(mediatype, L"image/png") || str::Eq(mediatype, L"image/jpeg") || str::Eq(mediatype, L"image/gif");
}

bool EpubDoc::Load() {
    if (!zip) {
        return false;
    }
    AutoFree container(zip->GetFileDataByName("META-INF/container.xml"));
    if (!container.data) {
        return false;
    }
    HtmlParser parser;
    HtmlElement* node = parser.ParseInPlace(container.data);
    if (!node) {
        return false;
    }

    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByNameNS("rootfile", EPUB_CONTAINER_NS);
    if (!node) {
        return false;
    }
    AutoFreeWstr contentPath(node->GetAttribute("full-path"));
    if (!contentPath) {
        return false;
    }
    url::DecodeInPlace(contentPath);

    // encrypted files will be ignored (TODO: support decryption)
    WStrList encList;
    AutoFree encryption(zip->GetFileDataByName("META-INF/encryption.xml"));
    if (encryption.data) {
        (void)parser.ParseInPlace(encryption.data);
        HtmlElement* cr = parser.FindElementByNameNS("CipherReference", EPUB_ENC_NS);
        while (cr) {
            WCHAR* uri = cr->GetAttribute("URI");
            if (uri) {
                url::DecodeInPlace(uri);
                encList.Append(uri);
            }
            cr = parser.FindElementByNameNS("CipherReference", EPUB_ENC_NS, cr);
        }
    }

    AutoFree content(zip->GetFileDataByName(contentPath));
    if (!content.data) {
        return false;
    }
    ParseMetadata(content.data);
    node = parser.ParseInPlace(content.data);
    if (!node) {
        return false;
    }
    node = parser.FindElementByNameNS("manifest", EPUB_OPF_NS);
    if (!node) {
        return false;
    }

    WCHAR* slashPos = str::FindCharLast(contentPath, '/');
    if (slashPos)
        *(slashPos + 1) = '\0';
    else
        *contentPath = '\0';

    WStrList idList, pathList;

    for (node = node->down; node; node = node->next) {
        AutoFreeWstr mediatype(node->GetAttribute("media-type"));
        if (isImageMediaType(mediatype)) {
            AutoFreeWstr imgPath = node->GetAttribute("href");
            if (!imgPath) {
                continue;
            }
            url::DecodeInPlace(imgPath);
            imgPath.Set(str::Join(contentPath, imgPath));
            if (encList.Contains(imgPath)) {
                continue;
            }
            // load the image lazily
            ImageData2 data = {0};
            auto tmp = strconv::WstrToUtf8(imgPath);
            data.fileName = (char*)tmp.data();
            data.fileId = zip->GetFileId(data.fileName);
            images.Append(data);
        } else if (isHtmlMediaType(mediatype)) {
            AutoFreeWstr htmlPath(node->GetAttribute("href"));
            if (!htmlPath) {
                continue;
            }
            url::DecodeInPlace(htmlPath);
            AutoFreeWstr htmlId(node->GetAttribute("id"));
            // EPUB 3 ToC
            AutoFreeWstr properties(node->GetAttribute("properties"));
            if (properties && str::Find(properties, L"nav") && str::Eq(mediatype, L"application/xhtml+xml")) {
                tocPath.Set(str::Join(contentPath, htmlPath));
            }

            AutoFreeWstr fullContentPath = str::Join(contentPath, htmlPath);
            if (encList.size() > 0 && encList.Contains(fullContentPath)) {
                continue;
            }
            if (htmlPath && htmlId) {
                idList.Append(htmlId.StealData());
                pathList.Append(htmlPath.StealData());
            }
        }
    }

    node = parser.FindElementByNameNS("spine", EPUB_OPF_NS);
    if (!node) {
        return false;
    }

    // EPUB 2 ToC
    AutoFreeWstr tocId(node->GetAttribute("toc"));
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
        AutoFreeWstr idref = node->GetAttribute("idref");
        if (!idref || !idList.Contains(idref)) {
            continue;
        }

        const WCHAR* fileName = pathList.at(idList.Find(idref));
        AutoFreeWstr fullPath = str::Join(contentPath, fileName);
        AutoFree html = zip->GetFileDataByName(fullPath);
        if (!html.data) {
            continue;
        }
        char* decoded = DecodeTextToUtf8(html.data, true);
        html.TakeOwnershipOf(decoded);
        if (!html.data) {
            continue;
        }
        // insert explicit page-breaks between sections including
        // an anchor with the file name at the top (for internal links)
        AutoFree utf8_path = strconv::WstrToUtf8(fullPath);
        DebugCrashIf(str::FindChar(utf8_path.Get(), '"'));
        str::TransChars(utf8_path.Get(), "\"", "'");
        htmlData.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", utf8_path.Get());
        htmlData.Append(html.data);
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
        if (tok->IsStartTag() && tok->NameIsNS("metadata", EPUB_OPF_NS))
            insideMetadata++;
        else if (tok->IsEndTag() && tok->NameIsNS("metadata", EPUB_OPF_NS))
            insideMetadata--;
        if (!insideMetadata)
            continue;
        if (!tok->IsStartTag())
            continue;

        for (int i = 0; i < dimof(metadataMap); i++) {
            // TODO: implement proper namespace support
            if (tok->NameIs(metadataMap[i].name) || Tag_Meta == tok->tag && tok->GetAttrByName("property") &&
                                                        tok->GetAttrByName("property")->ValIs(metadataMap[i].name)) {
                tok = pullParser.Next();
                if (tok && tok->IsText())
                    props.Set(metadataMap[i].prop, ResolveHtmlEntities(tok->s, tok->sLen));
                break;
            }
        }
    }
}

std::string_view EpubDoc::GetHtmlData() const {
    return htmlData.AsView();
}

ImageData* EpubDoc::GetImageData(const char* fileName, const char* pagePath) {
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
        for (size_t i = 0; i < images.size(); i++) {
            ImageData2* img = &images.at(i);
            if (str::EndsWithI(img->fileName, fileName)) {
                if (!img->base.data) {
                    auto res = zip->GetFileDataById(img->fileId);
                    img->base.len = res.size();
                    img->base.data = (char*)res.data();
                }
                if (img->base.data)
                    return &img->base;
            }
        }
        return nullptr;
    }

    AutoFree url(NormalizeURL(fileName, pagePath));
    // some EPUB producers use wrong path separators
    if (str::FindChar(url, '\\'))
        str::TransChars(url, "\\", "/");
    for (size_t i = 0; i < images.size(); i++) {
        ImageData2* img = &images.at(i);
        if (str::Eq(img->fileName, url)) {
            if (!img->base.data) {
                auto res = zip->GetFileDataById(img->fileId);
                img->base.len = res.size();
                img->base.data = (char*)res.data();
            }
            if (img->base.data)
                return &img->base;
        }
    }

    // try to also load images which aren't registered in the manifest
    ImageData2 data = {0};
    data.fileId = zip->GetFileId(url);
    if (data.fileId != (size_t)-1) {
        auto res = zip->GetFileDataById(data.fileId);
        data.base.len = res.size();
        data.base.data = (char*)res.data();
        if (data.base.data) {
            data.fileName = str::Dup(url);
            images.Append(data);
            return &images.Last().base;
        }
    }

    return nullptr;
}

std::string_view EpubDoc::GetFileData(const char* relPath, const char* pagePath) {
    if (!pagePath) {
        CrashIf(true);
        return {};
    }

    ScopedCritSec scope(&zipAccess);

    AutoFree url(NormalizeURL(relPath, pagePath));
    return zip->GetFileDataByName(url);
}

WCHAR* EpubDoc::GetProperty(DocumentProperty prop) const {
    return props.Get(prop);
}

const WCHAR* EpubDoc::GetFileName() const {
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
            if (attr && attr->ValIs("toc"))
                break;
        }
    }
    if (!tok || tok->IsError())
        return false;

    int level = 0;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() && (!tok->IsEndTag() || Tag_Nav != tok->tag)) {
        if (tok->IsStartTag() && Tag_Ol == tok->tag)
            level++;
        else if (tok->IsEndTag() && Tag_Ol == tok->tag && level > 0)
            level--;
        if (tok->IsStartTag() && (Tag_A == tok->tag || Tag_Span == tok->tag)) {
            HtmlTag itemTag = tok->tag;
            AutoFree text, href;
            if (Tag_A == tok->tag) {
                AttrInfo* attrInfo = tok->GetAttrByName("href");
                if (attrInfo)
                    href.Set(str::DupN(attrInfo->val, attrInfo->valLen));
            }
            while ((tok = parser.Next()) != nullptr && !tok->IsError() && (!tok->IsEndTag() || itemTag != tok->tag)) {
                if (tok->IsText()) {
                    AutoFree part(str::DupN(tok->s, tok->sLen));
                    if (!text) {
                        text.Set(part.release());
                    } else {
                        text.Set(str::Join(text, part));
                    }
                }
            }
            if (!text) {
                continue;
            }
            AutoFreeWstr itemText(strconv::Utf8ToWstr(text.get()));
            str::NormalizeWS(itemText);
            AutoFreeWstr itemSrc;
            if (href) {
                href.Set(NormalizeURL(href, pagePath));
                itemSrc.Set(strconv::FromHtmlUtf8(href, str::Len(href)));
            }
            visitor->Visit(itemText, itemSrc, level);
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
                visitor->Visit(itemText, itemSrc, level);
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
                AutoFree src(str::DupN(attrInfo->val, attrInfo->valLen));
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
        auto res = zip->GetFileDataByName(tocPath);
        tocDataLen = res.size();
        tocData.Set(res.data());
    }
    if (!tocData) {
        return false;
    }

    AutoFree pagePath(strconv::WstrToUtf8(tocPath));
    if (isNcxToc) {
        return ParseNcxToc(tocData, tocDataLen, pagePath.Get(), visitor);
    }
    return ParseNavToc(tocData, tocDataLen, pagePath.Get(), visitor);
}

bool EpubDoc::IsSupportedFile(const WCHAR* fileName, bool sniff) {
    if (!sniff) {
        return str::EndsWithI(fileName, L".epub");
    }
    AutoDelete<MultiFormatArchive> archive = OpenZipArchive(fileName, true);
    if (!archive.get()) {
        return false;
    }
    AutoFree mimetype(archive->GetFileDataByName("mimetype"));
    if (!mimetype.data) {
        return false;
    }
    char* d = mimetype.data;
    // trailing whitespace is allowed for the mimetype file
    for (size_t i = mimetype.size(); i > 0; i--) {
        if (!str::IsWs(d[i - 1])) {
            break;
        }
        d[i - 1] = '\0';
    }
    // a proper EPUB document has a "mimetype" file with content
    // "application/epub+zip" as the first entry in its ZIP structure
    /* cf. http://forums.fofou.org/sumatrapdf/topic?id=2599331
    if (!str::Eq(zip.GetFileName(0), L"mimetype"))
        return false; */
    return str::Eq(mimetype.data, "application/epub+zip") ||
           // also open renamed .ibooks files
           // cf. http://en.wikipedia.org/wiki/IBooks#Formats
           str::Eq(mimetype.data, "application/x-ibooks+zip");
}

EpubDoc* EpubDoc::CreateFromFile(const WCHAR* fileName) {
    EpubDoc* doc = new EpubDoc(fileName);
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

Fb2Doc::Fb2Doc(const WCHAR* fileName) : fileName(str::Dup(fileName)), stream(nullptr), isZipped(false), hasToc(false) {
}

Fb2Doc::Fb2Doc(IStream* stream) : fileName(nullptr), stream(stream), isZipped(false), hasToc(false) {
    stream->AddRef();
}

Fb2Doc::~Fb2Doc() {
    for (size_t i = 0; i < images.size(); i++) {
        free(images.at(i).base.data);
        free(images.at(i).fileName);
    }
    if (stream)
        stream->Release();
}

static std::string_view loadFromFile(Fb2Doc* doc) {
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

    std::string_view data;
    // if the ZIP file contains more than one file, we try to be rather
    // restrictive in what we accept in order not to accidentally accept
    // too many archives which only contain FB2 files among others:
    // the file must contain a single .fb2 file and may only contain
    // .url files in addition (TODO: anything else?)
    for (auto&& fileInfo : fileInfos) {
        auto fileName = fileInfo->name;
        const char* ext = path::GetExtNoFree(fileName.data());
        if (str::EqI(ext, ".fb2") && data.empty()) {
            data = archive->GetFileDataById(fileInfo->fileId);
        } else if (!str::EqI(ext, ".url")) {
            return {};
        }
    }
    return data;
}

static std::string_view loadFromStream(Fb2Doc* doc) {
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

    AutoFree data;
    if (fileName) {
        data = loadFromFile(this);
    } else if (stream) {
        data = loadFromStream(this);
    }
    if (data.empty()) {
        return false;
    }
    char* tmp = DecodeTextToUtf8(data.Get(), true);
    if (!tmp) {
        return false;
    }
    data.TakeOwnershipOf(tmp);

    HtmlPullParser parser(data.Get(), data.size());
    HtmlToken* tok;
    int inBody = 0, inTitleInfo = 0, inDocInfo = 0;
    const char* bodyStart = nullptr;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!inTitleInfo && !inDocInfo && tok->IsStartTag() && Tag_Body == tok->tag) {
            if (!inBody++)
                bodyStart = tok->s;
        } else if (inBody && tok->IsEndTag() && Tag_Body == tok->tag) {
            if (!--inBody) {
                if (xmlData.size() > 0)
                    xmlData.Append("<pagebreak />");
                xmlData.AppendChar('<');
                xmlData.Append(bodyStart, tok->s - bodyStart + tok->sLen);
                xmlData.AppendChar('>');
            }
        } else if (inBody && tok->IsStartTag() && Tag_Title == tok->tag)
            hasToc = true;
        else if (inBody)
            continue;
        else if (inTitleInfo && tok->IsEndTag() && tok->NameIsNS("title-info", FB2_MAIN_NS))
            inTitleInfo--;
        else if (inDocInfo && tok->IsEndTag() && tok->NameIsNS("document-info", FB2_MAIN_NS))
            inDocInfo--;
        else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("book-title", FB2_MAIN_NS)) {
            if ((tok = parser.Next()) == nullptr || tok->IsError())
                break;
            if (tok->IsText())
                props.Set(DocumentProperty::Title, ResolveHtmlEntities(tok->s, tok->sLen));
        } else if ((inTitleInfo || inDocInfo) && tok->IsStartTag() && tok->NameIsNS("author", FB2_MAIN_NS)) {
            AutoFree docAuthor;
            while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
                   !(tok->IsEndTag() && tok->NameIsNS("author", FB2_MAIN_NS))) {
                if (tok->IsText()) {
                    AutoFree author(ResolveHtmlEntities(tok->s, tok->sLen));
                    if (docAuthor) {
                        docAuthor.Set(str::Join(docAuthor, " ", author));
                    } else {
                        docAuthor.Set(author.release());
                    }
                }
            }
            if (docAuthor) {
                str::NormalizeWS(docAuthor);
                if (!str::IsEmpty(docAuthor.Get()))
                    props.Set(DocumentProperty::Author, docAuthor.release(), inTitleInfo != 0);
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("date", FB2_MAIN_NS)) {
            AttrInfo* attr = tok->GetAttrByNameNS("value", FB2_MAIN_NS);
            if (attr)
                props.Set(DocumentProperty::CreationDate, ResolveHtmlEntities(attr->val, attr->valLen));
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS("date", FB2_MAIN_NS)) {
            AttrInfo* attr = tok->GetAttrByNameNS("value", FB2_MAIN_NS);
            if (attr)
                props.Set(DocumentProperty::ModificationDate, ResolveHtmlEntities(attr->val, attr->valLen));
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS("program-used", FB2_MAIN_NS)) {
            if ((tok = parser.Next()) == nullptr || tok->IsError())
                break;
            if (tok->IsText())
                props.Set(DocumentProperty::CreatorApp, ResolveHtmlEntities(tok->s, tok->sLen));
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("coverpage", FB2_MAIN_NS)) {
            tok = parser.Next();
            if (tok && tok->IsText())
                tok = parser.Next();
            if (tok && tok->IsEmptyElementEndTag() && Tag_Image == tok->tag) {
                AttrInfo* attr = tok->GetAttrByNameNS("href", FB2_XLINK_NS);
                if (attr)
                    coverImage.Set(str::DupN(attr->val, attr->valLen));
            }
        } else if (inTitleInfo || inDocInfo)
            continue;
        else if (tok->IsStartTag() && tok->NameIsNS("title-info", FB2_MAIN_NS))
            inTitleInfo++;
        else if (tok->IsStartTag() && tok->NameIsNS("document-info", FB2_MAIN_NS))
            inDocInfo++;
        else if (tok->IsStartTag() && tok->NameIsNS("binary", FB2_MAIN_NS))
            ExtractImage(&parser, tok);
    }

    return xmlData.size() > 0;
}

void Fb2Doc::ExtractImage(HtmlPullParser* parser, HtmlToken* tok) {
    AutoFree id;
    AttrInfo* attrInfo = tok->GetAttrByNameNS("id", FB2_MAIN_NS);
    if (attrInfo) {
        id.Set(str::DupN(attrInfo->val, attrInfo->valLen));
        url::DecodeInPlace(id);
    }

    tok = parser->Next();
    if (!tok || !tok->IsText())
        return;

    ImageData2 data = {0};
    data.base.data = Base64Decode(tok->s, tok->sLen, &data.base.len);
    if (!data.base.data)
        return;
    data.fileName = str::Join("#", id);
    data.fileId = images.size();
    images.Append(data);
}

std::string_view Fb2Doc::GetXmlData() const {
    return {xmlData.Get(), xmlData.size()};
}

const char* Fb2Doc::GetXmlData(size_t* lenOut) const {
    *lenOut = xmlData.size();
    return xmlData.Get();
}

size_t Fb2Doc::GetXmlDataSize() const {
    return xmlData.size();
}

ImageData* Fb2Doc::GetImageData(const char* fileName) {
    for (size_t i = 0; i < images.size(); i++) {
        if (str::Eq(images.at(i).fileName, fileName))
            return &images.at(i).base;
    }
    return nullptr;
}

ImageData* Fb2Doc::GetCoverImage() {
    if (!coverImage)
        return nullptr;
    return GetImageData(coverImage);
}

WCHAR* Fb2Doc::GetProperty(DocumentProperty prop) const {
    return props.Get(prop);
}

const WCHAR* Fb2Doc::GetFileName() const {
    return fileName;
}

bool Fb2Doc::IsZipped() const {
    return isZipped;
}

bool Fb2Doc::HasToc() const {
    return hasToc;
}

bool Fb2Doc::ParseToc(EbookTocVisitor* visitor) {
    AutoFreeWstr itemText;
    bool inTitle = false;
    int titleCount = 0;
    int level = 0;

    size_t xmlLen;
    const char* xmlData = GetXmlData(&xmlLen);
    HtmlPullParser parser(xmlData, xmlLen);
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && Tag_Section == tok->tag)
            level++;
        else if (tok->IsEndTag() && Tag_Section == tok->tag && level > 0)
            level--;
        else if (tok->IsStartTag() && Tag_Title == tok->tag) {
            inTitle = true;
            titleCount++;
        } else if (tok->IsEndTag() && Tag_Title == tok->tag) {
            if (itemText)
                str::NormalizeWS(itemText);
            if (!str::IsEmpty(itemText.Get())) {
                AutoFreeWstr url(str::Format(TEXT(FB2_TOC_ENTRY_MARK) L"%d", titleCount));
                visitor->Visit(itemText, url, level);
                itemText.Reset();
            }
            inTitle = false;
        } else if (inTitle && tok->IsText()) {
            AutoFreeWstr text(strconv::FromHtmlUtf8(tok->s, tok->sLen));
            if (str::IsEmpty(itemText.Get()))
                itemText.Set(text.StealData());
            else
                itemText.Set(str::Join(itemText, L" ", text));
        }
    }

    return true;
}

bool Fb2Doc::IsSupportedFile(const WCHAR* fileName, bool sniff) {
    UNUSED(sniff);
    // TODO: implement sniffing
    return str::EndsWithI(fileName, L".fb2") || str::EndsWithI(fileName, L".fb2z") ||
           str::EndsWithI(fileName, L".zfb2") || str::EndsWithI(fileName, L".fb2.zip");
}

Fb2Doc* Fb2Doc::CreateFromFile(const WCHAR* fileName) {
    Fb2Doc* doc = new Fb2Doc(fileName);
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

PalmDoc::PalmDoc(const WCHAR* fileName) : fileName(str::Dup(fileName)) {
}

PalmDoc::~PalmDoc() {
}

#define PDB_TOC_ENTRY_MARK "ToC!Entry!"

// cf. http://wiki.mobileread.com/wiki/TealDoc
static const char* HandleTealDocTag(str::Str& builder, WStrVec& tocEntries, const char* text, size_t len,
                                    UINT codePage) {
    UNUSED(codePage);
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
    if (!tok || !tok->IsStartTag())
        goto Fallback;

    if (tok->NameIs("BOOKMARK")) {
        // <BOOKMARK NAME="Contents">
        AttrInfo* attr = tok->GetAttrByName("NAME");
        if (attr && attr->valLen > 0) {
            tocEntries.Append(strconv::FromHtmlUtf8(attr->val, attr->valLen));
            builder.AppendFmt("<a name=" PDB_TOC_ENTRY_MARK "%d>", tocEntries.size());
            return tok->s + tok->sLen;
        }
    } else if (tok->NameIs("HEADER")) {
        // <HEADER TEXT="Contents" ALIGN=CENTER STYLE=UNDERLINE>
        int hx = 2;
        AttrInfo* attr = tok->GetAttrByName("FONT");
        if (attr && attr->valLen > 0)
            hx = '0' == *attr->val ? 5 : '2' == *attr->val ? 1 : 3;
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
    if (!mobiDoc)
        return false;
    if (PdbDocType::PalmDoc != mobiDoc->GetDocType() && PdbDocType::TealDoc != mobiDoc->GetDocType()) {
        delete mobiDoc;
        return false;
    }

    const std::string_view text = mobiDoc->GetHtmlData();
    UINT codePage = GuessTextCodepage(text.data(), text.size(), CP_ACP);
    AutoFree textUtf8(strconv::ToMultiByte(text.data(), codePage, CP_UTF8));

    const char* start = textUtf8.Get();
    const char* end = start + textUtf8.size();
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
    return true;
}

std::string_view PalmDoc::GetHtmlData() const {
    return htmlData.AsView();
}

WCHAR* PalmDoc::GetProperty(DocumentProperty prop) const {
    UNUSED(prop);
    return nullptr;
}

const WCHAR* PalmDoc::GetFileName() const {
    return fileName;
}

bool PalmDoc::HasToc() const {
    return tocEntries.size() > 0;
}

bool PalmDoc::ParseToc(EbookTocVisitor* visitor) {
    for (size_t i = 0; i < tocEntries.size(); i++) {
        AutoFreeWstr name(str::Format(TEXT(PDB_TOC_ENTRY_MARK) L"%d", i + 1));
        visitor->Visit(tocEntries.at(i), name, 1);
    }
    return true;
}

bool PalmDoc::IsSupportedFile(const WCHAR* fileName, bool sniff) {
    if (!sniff) {
        bool isPdb = str::EndsWithI(fileName, L".pdb");
        bool isPrc = str::EndsWithI(fileName, L".prc");
        return isPdb || isPrc;
    }

    PdbReader pdbReader;
    auto data = file::ReadFile(fileName);
    if (!pdbReader.Parse(data)) {
        return false;
    }

    const char* kind = pdbReader.GetDbType();
    return str::Eq(kind, "TEXtREAd") || str::Eq(kind, "TEXtTlDc");
}

PalmDoc* PalmDoc::CreateFromFile(const WCHAR* fileName) {
    PalmDoc* doc = new PalmDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

/* ********** Plain HTML ********** */

HtmlDoc::HtmlDoc(const WCHAR* fileName) : fileName(str::Dup(fileName)) {
}

HtmlDoc::~HtmlDoc() {
    for (size_t i = 0; i < images.size(); i++) {
        free(images.at(i).base.data);
        free(images.at(i).fileName);
    }
}

bool HtmlDoc::Load() {
    AutoFree data(file::ReadFile(fileName));
    if (!data.data) {
        return false;
    }
    htmlData.Set(DecodeTextToUtf8(data.data, true));
    if (!htmlData)
        return false;

    pagePath.Set(strconv::WstrToUtf8(fileName).data());
    str::TransChars(pagePath, "\\", "/");

    HtmlPullParser parser(htmlData, str::Len(htmlData));
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsTag() || Tag_Body != tok->tag && Tag_P != tok->tag)) {
        if (tok->IsStartTag() && Tag_Title == tok->tag) {
            tok = parser.Next();
            if (tok && tok->IsText())
                props.Set(DocumentProperty::Title, ResolveHtmlEntities(tok->s, tok->sLen));
        } else if ((tok->IsStartTag() || tok->IsEmptyElementEndTag()) && Tag_Meta == tok->tag) {
            AttrInfo* attrName = tok->GetAttrByName("name");
            AttrInfo* attrValue = tok->GetAttrByName("content");
            if (!attrName || !attrValue)
                /* ignore this tag */;
            else if (attrName->ValIs("author"))
                props.Set(DocumentProperty::Author, ResolveHtmlEntities(attrValue->val, attrValue->valLen));
            else if (attrName->ValIs("date"))
                props.Set(DocumentProperty::CreationDate, ResolveHtmlEntities(attrValue->val, attrValue->valLen));
            else if (attrName->ValIs("copyright"))
                props.Set(DocumentProperty::Copyright, ResolveHtmlEntities(attrValue->val, attrValue->valLen));
        }
    }

    return true;
}

std::string_view HtmlDoc::GetHtmlData() {
    return htmlData.as_view();
}

ImageData* HtmlDoc::GetImageData(const char* fileName) {
    // TODO: this isn't thread-safe (might leak image data when called concurrently),
    //       so add a critical section once it's used for EbookController

    AutoFree url(NormalizeURL(fileName, pagePath));
    for (size_t i = 0; i < images.size(); i++) {
        if (str::Eq(images.at(i).fileName, url))
            return &images.at(i).base;
    }

    ImageData2 data = {0};
    auto urlData = LoadURL(url);
    if (urlData.empty()) {
        return nullptr;
    }
    data.base.data = (char*)urlData.data();
    data.base.len = urlData.size();
    data.fileName = url.release();
    images.Append(data);
    return &images.Last().base;
}

std::string_view HtmlDoc::GetFileData(const char* relPath) {
    AutoFree url(NormalizeURL(relPath, pagePath));
    return LoadURL(url);
}

std::string_view HtmlDoc::LoadURL(const char* url) {
    if (str::StartsWith(url, "data:")) {
        return DecodeDataURI(url);
    }
    if (str::FindChar(url, ':')) {
        return {};
    }
    AutoFreeWstr path(strconv::Utf8ToWstr(url));
    str::TransChars(path, L"/", L"\\");
    return file::ReadFile(path);
}

WCHAR* HtmlDoc::GetProperty(DocumentProperty prop) const {
    return props.Get(prop);
}

const WCHAR* HtmlDoc::GetFileName() const {
    return fileName;
}

bool HtmlDoc::IsSupportedFile(const WCHAR* fileName, bool sniff) {
    UNUSED(sniff);
    return str::EndsWithI(fileName, L".html") || str::EndsWithI(fileName, L".htm") ||
           str::EndsWithI(fileName, L".xhtml");
}

HtmlDoc* HtmlDoc::CreateFromFile(const WCHAR* fileName) {
    HtmlDoc* doc = new HtmlDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}

/* ********** Plain Text (and RFCs and TCR) ********** */

TxtDoc::TxtDoc(const WCHAR* fileName) : fileName(str::Dup(fileName)), isRFC(false) {
}

// cf. http://www.cix.co.uk/~gidds/Software/TCR.html
#define TCR_HEADER "!!8-Bit!!"

static char* DecompressTcrText(const char* data, size_t dataLen) {
    CrashIf(!str::StartsWith(data, TCR_HEADER));
    const char* curr = data + str::Len(TCR_HEADER);
    const char* end = data + dataLen;

    const char* dict[256];
    for (int n = 0; n < dimof(dict); n++) {
        if (curr >= end) {
            return str::Dup(data);
        }
        dict[n] = curr;
        curr += 1 + (uint8_t)*curr;
    }

    str::Str text(dataLen * 2);
    text.allowFailure = true;
    for (; curr < end; curr++) {
        const char* entry = dict[(uint8_t)*curr];
        bool ok = text.Append(entry + 1, (uint8_t)*entry);
        if (!ok) {
            return nullptr;
        }
    }

    return text.StealData();
}

static const char* TextFindLinkEnd(str::Str& htmlData, const char* curr, char prevChar, bool fromWww = false) {
    const char *end, *quote;

    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    for (end = curr; *end && !str::IsWs(*end); end++)
        ;
    if (',' == end[-1] || '.' == end[-1] || '?' == end[-1] || '!' == end[-1])
        end--;
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == end[-1] && (!str::FindChar(curr, '(') || str::FindChar(curr, '(') >= end))
        end--;
    // cut the link at the first quotation mark, if it's also preceded by one
    if (('"' == prevChar || '\'' == prevChar) && (quote = str::FindChar(curr, prevChar)) != nullptr && quote < end)
        end = quote;

    if (fromWww && (end - curr <= 4 || !str::FindChar(curr + 5, '.') || str::FindChar(curr + 5, '.') >= end)) {
        // ignore www. links without a top-level domain
        return nullptr;
    }

    htmlData.Append("<a href=\"");
    if (fromWww)
        htmlData.Append("http://");
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
    return isalnum((unsigned char)c) || c && str::FindChar(".!#$%&'*+=?^_`{|}~-", c);
}
inline bool IsEmailDomainChar(char c) {
    return isalnum((unsigned char)c) || '-' == c;
}

static const char* TextFindEmailEnd(str::Str& htmlData, const char* curr) {
    AutoFree beforeAt;
    const char* end = curr;
    if ('@' == *curr) {
        if (htmlData.size() == 0 || !IsEmailUsernameChar(htmlData.Last()))
            return nullptr;
        size_t idx = htmlData.size();
        for (; idx > 1 && IsEmailUsernameChar(htmlData.at(idx - 1)); idx--)
            ;
        beforeAt.SetCopy(&htmlData.at(idx));
    } else {
        CrashIf(!str::StartsWith(curr, "mailto:"));
        end = curr = curr + 7; // skip mailto:
        if (!IsEmailUsernameChar(*end))
            return nullptr;
        for (; IsEmailUsernameChar(*end); end++)
            ;
    }

    if (*end != '@' || !IsEmailDomainChar(*(end + 1)))
        return nullptr;
    for (end++; IsEmailDomainChar(*end); end++)
        ;
    if ('.' != *end || !IsEmailDomainChar(*(end + 1)))
        return nullptr;
    do {
        for (end++; IsEmailDomainChar(*end); end++)
            ;
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
    if (isalnum((unsigned char)*(curr - 1)))
        return nullptr;
    int rfc;
    const char* end = str::Parse(curr, "RFC %d", &rfc);
    // cf. http://en.wikipedia.org/wiki/Request_for_Comments#Obtaining_RFCs
    htmlData.AppendFmt("<a href='http://www.rfc-editor.org/rfc/rfc%d.txt'>", rfc);
    return end;
}

bool TxtDoc::Load() {
    AutoFree text(file::ReadFile(fileName));
    if (str::EndsWithI(fileName, L".tcr") && str::StartsWith(text.data, TCR_HEADER)) {
        text.TakeOwnershipOf(DecompressTcrText(text.data, text.size()));
    }
    if (!text.data) {
        return false;
    }
    text.TakeOwnershipOf(DecodeTextToUtf8(text.data));
    if (!text.data) {
        return false;
    }

    int rfc;
    isRFC = str::Parse(path::GetBaseNameNoFree(fileName), L"rfc%d.txt%$", &rfc) != nullptr;

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
        } else if (linkEnd)
            /* don't check for hyperlinks inside a link */;
        else if ('@' == *curr)
            linkEnd = TextFindEmailEnd(htmlData, curr);
        else if (curr > d && ('/' == curr[-1] || isalnum((unsigned char)curr[-1])))
            /* don't check for a link at this position */;
        else if ('h' == *curr && str::Parse(curr, "http%?s://"))
            linkEnd = TextFindLinkEnd(htmlData, curr, curr > d ? curr[-1] : ' ');
        else if ('w' == *curr && str::StartsWith(curr, "www."))
            linkEnd = TextFindLinkEnd(htmlData, curr, curr > d ? curr[-1] : ' ', true);
        else if ('m' == *curr && str::StartsWith(curr, "mailto:"))
            linkEnd = TextFindEmailEnd(htmlData, curr);
        else if (isRFC && curr > d && 'R' == *curr && str::Parse(curr, "RFC %d", &rfc))
            linkEnd = TextFindRfcEnd(htmlData, curr);

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
    if (linkEnd)
        htmlData.Append("</a>");
    htmlData.Append("</pre>");

    return true;
}

std::string_view TxtDoc::GetHtmlData() const {
    return htmlData.AsView();
}

WCHAR* TxtDoc::GetProperty(DocumentProperty prop) const {
    UNUSED(prop);
    return nullptr;
}

const WCHAR* TxtDoc::GetFileName() const {
    return fileName;
}

bool TxtDoc::IsRFC() const {
    return isRFC;
}

bool TxtDoc::HasToc() const {
    return isRFC;
}

static inline const WCHAR* SkipDigits(const WCHAR* s) {
    while (str::IsDigit(*s))
        s++;
    return s;
}

bool TxtDoc::ParseToc(EbookTocVisitor* visitor) {
    if (!isRFC)
        return false;

    HtmlParser parser;
    parser.Parse(htmlData.Get(), CP_UTF8);
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
        visitor->Visit(title, id, level);
    }

    return true;
}

bool TxtDoc::IsSupportedFile(const WCHAR* fileName, bool sniff) {
    UNUSED(sniff);
    return str::EndsWithI(fileName, L".txt") || str::EndsWithI(fileName, L".log") ||
           // http://en.wikipedia.org/wiki/.nfo
           str::EndsWithI(fileName, L".nfo") ||
           // http://en.wikipedia.org/wiki/FILE_ID.DIZ
           str::EndsWithI(fileName, L"\\file_id.diz") ||
           // http://en.wikipedia.org/wiki/Read.me
           str::EndsWithI(fileName, L"\\Read.me") ||
           // http://www.cix.co.uk/~gidds/Software/TCR.html
           str::EndsWithI(fileName, L".tcr");
}

TxtDoc* TxtDoc::CreateFromFile(const WCHAR* fileName) {
    TxtDoc* doc = new TxtDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return nullptr;
    }
    return doc;
}
