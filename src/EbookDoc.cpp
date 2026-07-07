/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Archive.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/HtmlTags.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "DocProperties.h"
#include "DocController.h"
#include "EbookBase.h"
#include "GumboHtmlParser.h"
#include "GumboHelpers.h"
#include "EbookDoc.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"

#if !OS_WIN
static uint GuessTextCodepage(Str, uint defVal) {
    return defVal;
}
#endif

static void SkipXmlPIAttrName(Str s, int& off) {
    while (off < s.len) {
        char c = s.s[off];
        if (str::IsWs(c) || c == '=' || c == '?' || c == '>') {
            return;
        }
        off++;
    }
}

static TempStr GetXmlPIAttrTemp(Str xmlPI, Str attrName) {
    int off = 2; // skip "<?"
    SkipNonWs(xmlPI, off);
    while (off < xmlPI.len) {
        SkipWs(xmlPI, off);
        if (off >= xmlPI.len || xmlPI.s[off] == '?' || xmlPI.s[off] == '>') {
            return {};
        }

        int nameStart = off;
        SkipXmlPIAttrName(xmlPI, off);
        Str name(xmlPI.s + nameStart, off - nameStart);
        SkipWs(xmlPI, off);
        if (off >= xmlPI.len || xmlPI.s[off] != '=') {
            continue;
        }
        off++;
        SkipWs(xmlPI, off);
        if (off >= xmlPI.len) {
            return {};
        }

        Str val;
        if (xmlPI.s[off] == '"' || xmlPI.s[off] == '\'') {
            char quote = xmlPI.s[off++];
            int valStart = off;
            if (!SkipUntil(xmlPI, off, quote)) {
                return {};
            }
            val = Str(xmlPI.s + valStart, off - valStart);
            off++;
        } else {
            int valStart = off;
            SkipNonWs(xmlPI, off);
            val = Str(xmlPI.s + valStart, off - valStart);
        }
        if (str::EqI(name, attrName)) {
            return str::DupTemp(val);
        }
    }
    return {};
}

// tries to extract an encoding from <?xml encoding="..."?>
// returns CP_ACP on failure
static uint GetCodepageFromPI(Str xmlPI) {
    if (!str::StartsWith(xmlPI, "<?xml")) {
        return CP_ACP;
    }
    int xmlPIEnd = str::IndexOf(xmlPI, StrL("?>"));
    if (xmlPIEnd < 0) {
        return CP_ACP;
    }
    TempStr encoding = GetXmlPIAttrTemp(Str(xmlPI.s, xmlPIEnd + 2), StrL("encoding"));
    if (!encoding) {
        return CP_ACP;
    }

    struct {
        Str namePart;
        uint codePage;
    } static encodings[] = {
        {"UTF", CP_UTF8},
        {"utf", CP_UTF8},
        {"1252", 1252},
        {"1251", 1251},
        // TODO: any other commonly used codepages?
    };
    for (int i = 0; i < dimof(encodings); i++) {
        if (str::Contains(encoding, encodings[i].namePart)) {
            return encodings[i].codePage;
        }
    }
    return CP_ACP;
}

static bool IsValidUtf8(Str string) {
    for (int i = 0; i < string.len; i++) {
        u8 c = (u8)string.s[i];
        int skip;
        if (c < 0x80) {
            skip = 0;
        } else if (c < 0xC0) {
            return false;
        } else if (c < 0xE0) {
            skip = 1;
        } else if (c < 0xF0) {
            skip = 2;
        } else if (c < 0xF5) {
            skip = 3;
        } else {
            return false;
        }
        while (skip-- > 0) {
            i++;
            if (i >= string.len || ((u8)string.s[i] & 0xC0) != 0x80) {
                return false;
            }
        }
    }
    return true;
}

static TempStr DecodeTextToUtf8Temp(Str s, bool isXML = false) {
    if (str::StartsWith(s, UTF8_BOM)) {
        return str::DupTemp(Str(s.s + 3, s.len - 3));
    }
    if (str::StartsWith(s, UTF16_BOM)) {
        s = Str(s.s + 2, s.len - 2);
        WStr ws = str::CastToWCHAR(s);
        return ToUtf8Temp(ws);
    }
    if (str::StartsWith(s, UTF16BE_BOM)) {
        // convert from utf16 big endian to utf16
        s = Str(s.s + 2, s.len - 2);
        int n = str::CastToWCHAR(s).len;
        for (int i = 0; i < n; i++) {
            int idx = i * 2;
            std::swap(s.s[idx], s.s[idx + 1]);
        }
        WStr ws = str::CastToWCHAR(s);
        return ToUtf8Temp(ws);
    }
    uint codePage = isXML ? GetCodepageFromPI(s) : CP_ACP;
    if (CP_ACP == codePage && IsValidUtf8(s)) {
        return str::DupTemp(s);
    }
    if (CP_ACP == codePage) {
        codePage = GuessTextCodepage(Str(s), CP_ACP);
    }
    return strconv::ToMultiByteTemp(s, codePage, CP_UTF8);
}

TempStr NormalizeURLTemp(Str url, Str base) {
    ReportIf(!url || !base);
    if (url.s[0] == '/' || str::ContainsChar(url, ':')) {
        return str::DupTemp(url);
    }

    Str baseEnd = str::SliceFromCharLast(base, '/');
    Str hash = str::SliceFromChar(base, '#');
    int basePathLen;
    if (url.s[0] == '#') {
        basePathLen = hash ? (int)(hash.s - base.s) : base.len;
    } else if (baseEnd && hash && hash.s < baseEnd.s) {
        // find the last '/' before the '#'
        basePathLen = 0;
        for (char* p = hash.s - 1; p >= base.s; p--) {
            if (*p == '/') {
                basePathLen = (int)(p - base.s + 1);
                break;
            }
        }
    } else if (baseEnd) {
        basePathLen = (int)(baseEnd.s - base.s + 1);
    } else {
        basePathLen = 0;
    }
    TempStr basePath = basePathLen > 0 ? str::DupTemp(Str(base.s, basePathLen)) : Str{};
    TempStr norm = str::JoinTemp(basePath, url);

    int dst = 0;
    for (int src = 0; src < norm.len; src++) {
        char c = norm.s[src];
        if (c != '/') {
            norm.s[dst++] = c;
        } else if (str::StartsWith(Str(norm.s + src, norm.len - src), "/./")) {
            src++;
        } else if (str::StartsWith(Str(norm.s + src, norm.len - src), "/../")) {
            while (dst > 0 && norm.s[dst - 1] != '/') {
                dst--;
            }
            src += 3;
        } else {
            norm.s[dst++] = '/';
        }
    }
    norm.s[dst] = '\0';
    norm.len = dst;
    return norm;
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

static TempStr Base64DecodeTemp(Str data) {
    int sLen = data.len;
    char* s = data.s;
    char* end = data.s + sLen;
    char* result = AllocArrayTemp<char>(sLen * 3 / 4);
    char* curr = result;
    char c = 0;
    int step = 0;
    for (; s < end && *s != '='; s++) {
        char n = decode64(*s);
        if (-1 == n) {
            if (str::IsWs(*s)) {
                continue;
            }
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
    int size = (int)(curr - result);
    return Str(result, size);
}

static inline void AppendChar(str::Builder& htmlData, char c) {
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

static TempStr DecodeDataURITemp(Str url) {
    Str comma = str::SliceFromChar(url, ',');
    if (!comma) {
        return {};
    }
    Str data = Str(comma.s + 1, (int)(url.s + url.len - (comma.s + 1)));
    if ((int)(comma.s - url.s) >= 12 && str::EqN(Str(comma.s - 7, 7), StrL(";base64"), 7)) {
        return Base64DecodeTemp(data);
    }
    return str::DupTemp(data);
}

struct GumboDoc {
    GumboOptions opts;
    GumboOutput* output = nullptr;

    GumboDoc(Str data, bool xmlFragment) {
        opts = xmlFragment ? GumboMakeXmlFragmentOptions() : GumboMakeOptions();
        if (!data) {
            return;
        }
        output = gumbo_parse_with_options(&opts, data.s, (size_t)data.len);
    }

    ~GumboDoc() {
        if (output) {
            gumbo_destroy_output_iter(&opts, output);
        }
    }

    const GumboNode* Document() const { return output ? output->document : nullptr; }
};

static const GumboVector* GumboChildrenOf(const GumboNode* node) {
    if (!node) {
        return nullptr;
    }
    if (node->type == GUMBO_NODE_ELEMENT) {
        return &node->v.element.children;
    }
    if (node->type == GUMBO_NODE_DOCUMENT) {
        return &node->v.document.children;
    }
    return nullptr;
}

/* ********** EPUB ********** */

static Str EPUB_CONTAINER_NS() {
    return StrL("urn:oasis:names:tc:opendocument:xmlns:container");
}
static Str EPUB_OPF_NS() {
    return StrL("http://www.idpf.org/2007/opf");
}
static Str EPUB_NCX_NS() {
    return StrL("http://www.daisy.org/z3986/2005/ncx/");
}
static Str EPUB_ENC_NS() {
    return StrL("http://www.w3.org/2001/04/xmlenc#");
}

EpubDoc::EpubDoc(Str fileName) {
    str::ReplaceWithCopy(&this->fileName, fileName);
    archive = OpenArchiveFromFile(fileName, /*eagerLoad=*/true, gArchiveProgressCb);
}

EpubDoc::~EpubDoc() {
    zipAccess.Lock();

    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName);
    }

    zipAccess.Unlock();
    delete archive;
    str::Free(tocPath);
    str::Free(fileName);
}

// TODO: switch to seqstring
static bool isHtmlMediaType(Str mediatype) {
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
static bool isImageMediaType(Str mediatype) {
    return str::Eq(mediatype, "image/png") || str::Eq(mediatype, "image/jpeg") || str::Eq(mediatype, "image/gif");
}

static void ParseMetadata(Str content, Props& props);

static void CollectEncryptedEpubPaths(const GumboNode* root, StrVec& encList) {
    Vec<const GumboNode*> toVisit;
    toVisit.Append(root);
    while (len(toVisit) > 0) {
        const GumboNode* node = toVisit.Pop();
        if (!node) {
            continue;
        }
        if (GumboTagNameIsNS(node, StrL("CipherReference"), EPUB_ENC_NS())) {
            TempStr uri = GumboAttributeValueTemp(node, "URI");
            if (uri) {
                url::DecodeInPlace(uri);
                encList.Append(uri);
            }
        }
        const GumboVector* children = GumboChildrenOf(node);
        if (!children) {
            continue;
        }
        for (unsigned int i = children->length; i > 0; i--) {
            toVisit.Append((const GumboNode*)children->data[i - 1]);
        }
    }
}

bool EpubDoc::Load() {
    if (!archive) {
        return false;
    }
    auto* containerFi = archive->GetFileDataByName("META-INF/container.xml");
    if (!containerFi || !containerFi->data) {
        return false;
    }
    Str container = Str((char*)((u8*)containerFi->data), containerFi->fileSizeUncompressed);
    GumboDoc containerDoc(container, true);
    const GumboNode* node = containerDoc.Document();
    if (!node) {
        return false;
    }

    // only consider the first <rootfile> element (default rendition)
    node = GumboFindDescendantByTagNS(node, StrL("rootfile"), EPUB_CONTAINER_NS());
    if (!node) {
        return false;
    }
    TempStr contentPath = GumboAttributeValueTemp(node, "full-path");
    if (!contentPath) {
        return false;
    }
    url::DecodeInPlace(contentPath);

    // encrypted files will be ignored (TODO: support decryption)
    StrVec encList;
    auto* encryptionFi = archive->GetFileDataByName("META-INF/encryption.xml");
    if (encryptionFi && encryptionFi->data) {
        Str encryption = Str((char*)((u8*)encryptionFi->data), encryptionFi->fileSizeUncompressed);
        GumboDoc encryptionDoc(encryption, true);
        CollectEncryptedEpubPaths(encryptionDoc.Document(), encList);
    }

    auto* contentFi = archive->GetFileDataByName(contentPath);
    if (!contentFi || !contentFi->data) {
        return false;
    }
    Str content = Str((char*)((u8*)contentFi->data), contentFi->fileSizeUncompressed);
    ParseMetadata(content, props);
    GumboDoc contentDoc(content, true);
    node = contentDoc.Document();
    if (!node) {
        return false;
    }
    node = GumboFindDescendantByTagNS(node, StrL("manifest"), EPUB_OPF_NS());
    if (!node) {
        return false;
    }

    int slashPos = str::LastIndexOfChar(contentPath, '/');
    if (slashPos >= 0) {
        contentPath = str::DupTemp(Str(contentPath.s, slashPos + 1));
    } else {
        contentPath = {};
    }

    StrVec idList, pathList;

    const GumboNode* manifest = node;
    const GumboVector* manifestChildren = GumboChildrenOf(manifest);
    for (unsigned int i = 0; manifestChildren && i < manifestChildren->length; i++) {
        node = (const GumboNode*)manifestChildren->data[i];
        if (!node || node->type != GUMBO_NODE_ELEMENT) {
            continue;
        }
        TempStr mediaType = GumboAttributeValueTemp(node, "media-type");
        if (isImageMediaType(mediaType)) {
            TempStr imgPath = GumboAttributeValueTemp(node, "href");
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
            data.fileId = archive->GetFileId(data.fileName);
            images.Append(data);
        } else if (isHtmlMediaType(mediaType)) {
            TempStr htmlPath = GumboAttributeValueTemp(node, "href");
            if (!htmlPath) {
                continue;
            }
            url::DecodeInPlace(htmlPath);
            TempStr htmlId = GumboAttributeValueTemp(node, "id");
            // EPUB 3 ToC
            TempStr properties = GumboAttributeValueTemp(node, "properties");
            if (properties && str::Contains(properties, StrL("nav")) && str::Eq(mediaType, "application/xhtml+xml")) {
                str::Free(tocPath);
                tocPath = str::Join(contentPath, htmlPath);
            }

            TempStr fullContentPath = str::JoinTemp(contentPath, htmlPath);
            if (encList.Contains(fullContentPath)) {
                continue;
            }
            if (htmlPath && htmlId) {
                idList.Append(htmlId);
                pathList.Append(htmlPath);
            }
        }
    }

    node = GumboFindDescendantByTagNS(contentDoc.Document(), StrL("spine"), EPUB_OPF_NS());
    if (!node) {
        return false;
    }

    // EPUB 2 ToC
    TempStr tocId = GumboAttributeValueTemp(node, "toc");
    int tocIdx = (tocId && len(tocPath) == 0) ? idList.Find(tocId) : -1;
    if (tocIdx >= 0) {
        Str s = pathList[tocIdx];
        str::Free(tocPath);
        tocPath = str::Join(contentPath, s);
        isNcxToc = true;
    }
    TempStr readingDir = GumboAttributeValueTemp(node, "page-progression-direction");
    if (readingDir) {
        isRtlDoc = str::EqI(readingDir, "rtl");
    }

    const GumboNode* spine = node;
    const GumboVector* spineChildren = GumboChildrenOf(spine);
    for (unsigned int i = 0; spineChildren && i < spineChildren->length; i++) {
        node = (const GumboNode*)spineChildren->data[i];
        if (!GumboTagNameIsNS(node, StrL("itemref"), EPUB_OPF_NS())) {
            continue;
        }
        TempStr idref = GumboAttributeValueTemp(node, "idref");
        if (!idref) {
            continue;
        }
        int idx = idList.Find(idref);
        if (idx < 0) {
            continue;
        }
        Str fname = pathList[idx];
        TempStr fullPath = str::JoinTemp(contentPath, fname);
        auto* htmlFi = archive->GetFileDataByName(fullPath);
        if (!htmlFi || !htmlFi->data) {
            continue;
        }
        Str html = Str((char*)((u8*)htmlFi->data), htmlFi->fileSizeUncompressed);
        TempStr decoded = DecodeTextToUtf8Temp(html, true);
        if (!decoded) {
            continue;
        }
        // insert explicit page-breaks between sections including
        // an anchor with the file name at the top (for internal links)
        ReportIf(str::ContainsChar(fullPath, '"'));
        str::TransCharsInPlace(fullPath, StrL("\""), StrL("'"));
        htmlData.Append(fmt("<pagebreak page_path=\"%s\" page_marker />", fullPath));
        htmlData.Append(decoded);
    }

    return len(htmlData) > 0;
}

static SeqStrNum epubPropsMap =
    "dc:title\0"
    "\x02"
    "dc:creator\0"
    "\x04"
    "dc:date\0"
    "\x0a"
    "dcterms:modified\0"
    "\x0c"
    "dc:description\0"
    "\x08"
    "dc:rights\0"
    "\x06"
    "\0";

static bool IsTokPropName(HtmlToken* tok, Str name) {
    if (tok->NameIs(name)) {
        return true;
    }
    if (Tag_Meta != tok->tag) {
        return false;
    }
    AttrInfo* attr = tok->GetAttrByName(StrL("property"));
    return attr && attr->ValIs(name);
}

static void ParseMetadata(Str content, Props& props) {
    GumboHtmlParser pullParser(content);
    int insideMetadata = 0;
    HtmlToken* tok;

    while ((tok = pullParser.Next()) != nullptr) {
        if (tok->IsStartTag() && tok->NameIsNS(StrL("metadata"), EPUB_OPF_NS())) {
            insideMetadata++;
        } else if (tok->IsEndTag() && tok->NameIsNS(StrL("metadata"), EPUB_OPF_NS())) {
            insideMetadata--;
        }
        if (!insideMetadata) {
            continue;
        }
        if (!tok->IsStartTag()) {
            continue;
        }

        int off = 0;
        while (Str epubName = SeqStrNumAt(epubPropsMap, off)) {
            // TODO: implement proper namespace support
            if (!IsTokPropName(tok, epubName)) {
                if (!SeqStrNumAdvance(epubPropsMap, off)) {
                    break;
                }
                continue;
            }
            tok = pullParser.Next();
            if (tok && tok->IsText()) {
                i64 propNo = 0;
                SeqStrNumIndex(epubPropsMap, epubName, &propNo);
                TempStr val = ResolveHtmlEntitiesTemp(tok->s);
                AddProp(props, (DocProp)propNo, val);
            }
            break;
        }
    }
}

Str EpubDoc::GetHtmlData() const {
    return ToStr(htmlData);
}

Str EpubDoc::GetImageData(Str fileName, Str pagePath) {
    ScopedMutex scope(&zipAccess);

    if (!pagePath) {
        ReportIf(true);
        // if we're reparsing, we might not have pagePath, which is needed to
        // build the exact url so try to find a partial match
        // TODO: the correct approach would be to extend reparseIdx into a
        // struct ReparseData, which would include pagePath and all other
        // styling related state (such as nextPageStyle, listDepth, etc. including
        // format specific state such as hiddenDepth and titleCount) and store it
        // in every HtmlPage, but this should work well enough for now
        for (ImageData& img : images) {
            if (str::EndsWithI(img.fileName, fileName)) {
                if (len(img.base) == 0) {
                    auto* fi = archive->GetFileDataById(img.fileId);
                    if (fi && fi->data) {
                        img.base = Str((char*)((u8*)fi->data), fi->fileSizeUncompressed);
                        fi->data = nullptr;
                    }
                }
                if (len(img.base) > 0) {
                    return img.base;
                }
            }
        }
        return {};
    }

    TempStr url = NormalizeURLTemp(fileName, pagePath);
    // some EPUB producers use wrong path separators
    if (str::ContainsChar(url, '\\')) {
        str::TransCharsInPlace(url, StrL("\\"), StrL("/"));
    }
    for (ImageData& img : images) {
        if (str::Eq(img.fileName, url)) {
            if (len(img.base) == 0) {
                auto* fi = archive->GetFileDataById(img.fileId);
                if (fi && fi->data) {
                    img.base = Str((char*)((u8*)fi->data), fi->fileSizeUncompressed);
                    fi->data = nullptr;
                }
            }
            if (len(img.base) > 0) {
                return img.base;
            }
        }
    }

    // try to also load images which aren't registered in the manifest
    ImageData data;
    data.fileId = archive->GetFileId(url);
    if (data.fileId >= 0) {
        auto* fi = archive->GetFileDataById(data.fileId);
        if (fi && fi->data) {
            data.base = Str((char*)((u8*)fi->data), fi->fileSizeUncompressed);
            fi->data = nullptr;
            data.fileName = str::Dup(url);
            images.Append(data);
            return images.Last().base;
        }
    }

    return {};
}

Str EpubDoc::GetFileData(Str relPath, Str pagePath) {
    if (!pagePath) {
        ReportIf(true);
        return {};
    }

    ScopedMutex scope(&zipAccess);

    TempStr url = NormalizeURLTemp(relPath, pagePath);
    auto* fi = archive->GetFileDataByName(url);
    if (!fi || !fi->data) {
        return {};
    }
    Str res = Str((char*)((u8*)fi->data), fi->fileSizeUncompressed);
    fi->data = nullptr;
    return res;
}

TempStr EpubDoc::GetPropertyTemp(DocProp prop) const {
    return GetPropValueTemp(props, prop);
}

Str EpubDoc::GetFileName() const {
    return fileName;
}

bool EpubDoc::IsRTL() const {
    return isRtlDoc;
}

bool EpubDoc::HasToc() const {
    return len(tocPath) > 0;
}

static bool ParseNavToc(Str data, Str pagePath, EbookTocVisitor* visitor) {
    GumboHtmlParser parser(data);
    HtmlToken* tok;
    // skip to the start of the <nav epub:type="toc">
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && Tag_Nav == tok->tag) {
            AttrInfo* attr = tok->GetAttrByName(StrL("epub:type"));
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
            TempStr text, href;
            if (Tag_A == tok->tag) {
                AttrInfo* attrInfo = tok->GetAttrByName(StrL("href"));
                if (attrInfo) {
                    href = str::DupTemp(attrInfo->val);
                }
            }
            while ((tok = parser.Next()) != nullptr && !tok->IsError() && (!tok->IsEndTag() || itemTag != tok->tag)) {
                if (tok->IsText()) {
                    TempStr part = str::DupTemp(tok->s);
                    if (!text) {
                        text = part;
                    } else {
                        text = str::JoinTemp(text, part);
                    }
                }
            }
            if (!text) {
                continue;
            }
            TempStr itemText = str::DupTemp(text);
            itemText.len -= str::NormalizeWSInPlace(itemText);
            TempStr itemSrc;
            if (href) {
                TempStr normHref = NormalizeURLTemp(href, pagePath);
                itemSrc = strconv::HtmlUtf8ToStrTemp(normHref);
            }
            visitor->Visit(itemText, itemSrc, level);
        }
    }

    return true;
}

static bool ParseNcxToc(Str data, Str pagePath, EbookTocVisitor* visitor) {
    GumboHtmlParser parser(data);
    HtmlToken* tok;
    // skip to the start of the navMap
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && tok->NameIsNS(StrL("navMap"), EPUB_NCX_NS())) {
            break;
        }
    }
    if (!tok || tok->IsError()) {
        return false;
    }

    TempStr itemText, itemSrc;
    int level = 0;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsEndTag() || !tok->NameIsNS(StrL("navMap"), EPUB_NCX_NS()))) {
        if (tok->IsTag() && tok->NameIsNS(StrL("navPoint"), EPUB_NCX_NS())) {
            if (itemText) {
                visitor->Visit(itemText, itemSrc, level);
                itemText = {};
                itemSrc = {};
            }
            if (tok->IsStartTag()) {
                level++;
            } else if (tok->IsEndTag() && level > 0) {
                level--;
            }
        } else if (tok->IsStartTag() && tok->NameIsNS(StrL("text"), EPUB_NCX_NS())) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                itemText = strconv::HtmlUtf8ToStrTemp(tok->s);
            }
        } else if (tok->IsTag() && !tok->IsEndTag() && tok->NameIsNS(StrL("content"), EPUB_NCX_NS())) {
            AttrInfo* attrInfo = tok->GetAttrByName(StrL("src"));
            if (attrInfo) {
                TempStr src = NormalizeURLTemp(attrInfo->val, pagePath);
                itemSrc = strconv::HtmlUtf8ToStrTemp(src);
            }
        }
    }

    return true;
}

bool EpubDoc::ParseToc(EbookTocVisitor* visitor) {
    if (len(tocPath) == 0) {
        return false;
    }
    Str tocDataStr;
    {
        ScopedMutex scope(&zipAccess);
        auto* fi = archive->GetFileDataByName(tocPath);
        if (fi && fi->data) {
            tocDataStr = Str(fi->data, fi->fileSizeUncompressed);
        }
    }
    if (!tocDataStr) {
        return false;
    }

    Str pagePath = tocPath;
    if (isNcxToc) {
        return ParseNcxToc(tocDataStr, pagePath, visitor);
    }
    return ParseNavToc(tocDataStr, pagePath, visitor);
}

bool EpubDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFileEpub;
}

EpubDoc* EpubDoc::CreateFromFile(Str path) {
    EpubDoc* doc = new EpubDoc(path);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

EpubDoc* EpubDoc::CreateFromData(Str data) {
    EpubDoc* doc = new EpubDoc(Str());
    doc->archive = OpenArchiveFromData(data);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

/* ********** FictionBook (FB2) ********** */

static Str FB2_MAIN_NS() {
    return StrL("http://www.gribuser.ru/xml/fictionbook/2.0");
}
static Str FB2_XLINK_NS() {
    return StrL("http://www.w3.org/1999/xlink");
}

Fb2Doc::Fb2Doc(Str fileName) : fileName(str::Dup(fileName)) {}

Fb2Doc::~Fb2Doc() {
    str::Free(coverImage);
    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName);
    }
    str::Free(fileName);
}

static Str takeFileData(Archive* archive, int fileId) {
    auto* fi = archive->GetFileDataById(fileId);
    if (!fi || !fi->data) {
        return {};
    }
    Str res = Str((char*)((u8*)fi->data), fi->fileSizeUncompressed);
    fi->data = nullptr;
    return res;
}

static Str loadFromFile(Fb2Doc* doc) {
    Archive* archive = OpenArchiveFromFile(doc->fileName, /*eagerLoad=*/true, gArchiveProgressCb);
    if (!archive) {
        return file::ReadFile(doc->fileName);
    }

    AutoDelete delArchive(archive);

    // we have archive with more than 1 file
    doc->isZipped = true;
    auto& fileInfos = archive->GetFileInfos();
    int nFiles = len(fileInfos);

    if (nFiles == 0) {
        return {};
    }

    if (nFiles == 1) {
        return takeFileData(archive, 0);
    }

    Str data;
    // if the ZIP file contains more than one file, we try to be rather
    // restrictive in what we accept in order not to accidentally accept
    // too many archives which only contain FB2 files among others:
    // the file must contain a single .fb2 file and may only contain
    // .url files in addition (TODO: anything else?)
    for (auto&& fileInfo : fileInfos) {
        auto path = fileInfo->name;
        if (str::EndsWithI(path, ".fb2") && len(data) == 0) {
            data = takeFileData(archive, fileInfo->fileId);
        } else if (!str::EndsWithI(path, ".url")) {
            return {};
        }
    }
    return data;
}

static Str loadFromData(Fb2Doc* doc, Str srcData) {
    Archive* archive = OpenArchiveFromData(srcData);
    if (!archive) {
        return str::Dup(srcData);
    }

    AutoDelete delArchive(archive);
    doc->isZipped = true;
    auto& fileInfos = archive->GetFileInfos();
    int nFiles = len(fileInfos);
    if (nFiles == 0) {
        return {};
    }
    if (nFiles == 1) {
        return takeFileData(archive, 0);
    }

    Str data;
    for (auto&& fileInfo : fileInfos) {
        auto path = fileInfo->name;
        if (str::EndsWithI(path, ".fb2") && len(data) == 0) {
            data = takeFileData(archive, fileInfo->fileId);
        } else if (!str::EndsWithI(path, ".url")) {
            str::Free(data);
            return {};
        }
    }
    return data;
}

bool Fb2Doc::Load(Str srcData) {
    ReportIf(!srcData && len(fileName) == 0);

    Str data;
    if (len(fileName) > 0) {
        data = loadFromFile(this);
    } else if (srcData) {
        data = loadFromData(this, srcData);
    }
    if (len(data) == 0) {
        return false;
    }
    TempStr tmp = DecodeTextToUtf8Temp(data, true);
    str::Free(data);
    if (!tmp) {
        return false;
    }

    Str data2 = Str((char*)((u8*)tmp.s), tmp.len);

    GumboHtmlParser parser(data2);
    HtmlToken* tok;
    int inBody = 0, inTitleInfo = 0, inDocInfo = 0;
    Str bodyStart;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!inTitleInfo && !inDocInfo && tok->IsStartTag() && Tag_Body == tok->tag) {
            if (!inBody++) {
                bodyStart = tok->s;
            }
        } else if (inBody && tok->IsEndTag() && Tag_Body == tok->tag) {
            if (!--inBody) {
                if (len(xmlData) > 0) {
                    xmlData.Append("<pagebreak />");
                }
                xmlData.AppendChar('<');
                xmlData.Append(Str(bodyStart.s, (int)(tok->s.s - bodyStart.s) + tok->s.len));
                xmlData.AppendChar('>');
            }
        } else if (inBody && tok->IsStartTag() && Tag_Title == tok->tag) {
            hasToc = true;
        } else if (inBody) {
            continue;
        } else if (inTitleInfo && tok->IsEndTag() && tok->NameIsNS(StrL("title-info"), FB2_MAIN_NS())) {
            inTitleInfo--;
        } else if (inDocInfo && tok->IsEndTag() && tok->NameIsNS(StrL("document-info"), FB2_MAIN_NS())) {
            inDocInfo--;
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS(StrL("book-title"), FB2_MAIN_NS())) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                TempStr val = ResolveHtmlEntitiesTemp(tok->s);
                AddProp(props, DocProp::Title, val);
            }
        } else if ((inTitleInfo || inDocInfo) && tok->IsStartTag() && tok->NameIsNS(StrL("author"), FB2_MAIN_NS())) {
            TempStr docAuthor = nullptr;
            while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
                   !(tok->IsEndTag() && tok->NameIsNS(StrL("author"), FB2_MAIN_NS()))) {
                if (tok->IsText()) {
                    TempStr author = ResolveHtmlEntitiesTemp(tok->s);
                    if (docAuthor) {
                        docAuthor = str::JoinTemp(docAuthor, StrL(" "), author);
                    } else {
                        docAuthor = author;
                    }
                }
            }
            if (docAuthor) {
                docAuthor.len -= str::NormalizeWSInPlace(docAuthor);
                if (len(docAuthor) > 0) {
                    TempStr val = docAuthor;
                    bool replaceIfExists = inTitleInfo != 0;
                    AddProp(props, DocProp::Author, val, replaceIfExists);
                }
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS(StrL("date"), FB2_MAIN_NS())) {
            AttrInfo* attr = tok->GetAttrByNameNS(StrL("value"), FB2_MAIN_NS());
            if (attr) {
                TempStr val = ResolveHtmlEntitiesTemp(attr->val);
                AddProp(props, DocProp::CreationDate, val);
            }
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS(StrL("date"), FB2_MAIN_NS())) {
            AttrInfo* attr = tok->GetAttrByNameNS(StrL("value"), FB2_MAIN_NS());
            if (attr) {
                TempStr val = ResolveHtmlEntitiesTemp(attr->val);
                AddProp(props, DocProp::ModificationDate, val);
            }
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS(StrL("program-used"), FB2_MAIN_NS())) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                TempStr val = ResolveHtmlEntitiesTemp(tok->s);
                AddProp(props, DocProp::CreatorApp, val);
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS(StrL("coverpage"), FB2_MAIN_NS())) {
            tok = parser.Next();
            if (tok && tok->IsText()) {
                tok = parser.Next();
            }
            if (tok && tok->IsEmptyElementEndTag() && Tag_Image == tok->tag) {
                AttrInfo* attr = tok->GetAttrByNameNS(StrL("href"), FB2_XLINK_NS());
                if (attr) {
                    str::ReplaceWithCopy(&coverImage, attr->val);
                }
            }
        } else if (inTitleInfo || inDocInfo) {
            continue;
        } else if (tok->IsStartTag() && tok->NameIsNS(StrL("title-info"), FB2_MAIN_NS())) {
            inTitleInfo++;
        } else if (tok->IsStartTag() && tok->NameIsNS(StrL("document-info"), FB2_MAIN_NS())) {
            inDocInfo++;
        } else if (tok->IsStartTag() && tok->NameIsNS(StrL("binary"), FB2_MAIN_NS())) {
            ExtractImage(&parser, tok);
        }
    }

    return len(xmlData) > 0;
}

void Fb2Doc::ExtractImage(GumboHtmlParser* parser, HtmlToken* tok) {
    TempStr id;
    AttrInfo* attrInfo = tok->GetAttrByNameNS(StrL("id"), FB2_MAIN_NS());
    if (attrInfo) {
        id = str::DupTemp(attrInfo->val);
        url::DecodeInPlace(id);
        id = Str(id.s); // DecodeInPlace shortens the buffer in place; re-read its length
    }

    tok = parser->Next();
    if (!tok || !tok->IsText()) {
        return;
    }

    TempStr decoded = Base64DecodeTemp(tok->s);
    if (len(decoded) == 0) {
        return;
    }
    ImageData data;
    data.base = str::Dup(decoded);
    data.fileName = str::Join(StrL("#"), id);
    data.fileId = len(images);
    images.Append(data);
}

Str Fb2Doc::GetXmlData() const {
    Str s = ToStr(xmlData);
    return Str((char*)((u8*)s.s), (int)((size_t)len(xmlData)));
}

Str Fb2Doc::GetImageData(Str fileName) const {
    for (int i = 0; i < len(images); i++) {
        if (str::Eq(images[i].fileName, fileName)) {
            return images[i].base;
        }
    }
    return {};
}

Str Fb2Doc::GetCoverImage() const {
    if (len(coverImage) == 0) {
        return {};
    }
    return GetImageData(coverImage);
}

TempStr Fb2Doc::GetPropertyTemp(DocProp prop) const {
    return GetPropValueTemp(props, prop);
}

Str Fb2Doc::GetFileName() const {
    return fileName;
}

bool Fb2Doc::IsZipped() const {
    return isZipped;
}

bool Fb2Doc::HasToc() const {
    return hasToc;
}

bool Fb2Doc::ParseToc(EbookTocVisitor* visitor) const {
    TempStr itemText;
    bool inTitle = false;
    int titleCount = 0;
    int level = 0;

    auto xmlData2 = GetXmlData();
    GumboHtmlParser parser(xmlData2);
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
            // NormalizeWSInPlace shortens the buffer in place; adjust len to match
            itemText.len -= str::NormalizeWSInPlace(itemText);
            if (len(itemText) > 0) {
                TempStr url = fmt(FB2_TOC_ENTRY_MARK "%d", titleCount);
                visitor->Visit(itemText, url, level);
                itemText = {};
            }
            inTitle = false;
        } else if (inTitle && tok->IsText()) {
            TempStr text = strconv::HtmlUtf8ToStrTemp(tok->s);
            if (len(itemText) == 0) {
                itemText = text;
            } else {
                itemText = str::JoinTemp(itemText, " ", text);
            }
        }
    }

    return true;
}

bool Fb2Doc::IsSupportedFileType(Kind kind) {
    return kind == kindFileFb2 || kind == kindFileFb2z;
}

Fb2Doc* Fb2Doc::CreateFromFile(Str path) {
    Fb2Doc* doc = new Fb2Doc(path);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

Fb2Doc* Fb2Doc::CreateFromData(Str data) {
    Fb2Doc* doc = new Fb2Doc(Str());
    if (!doc || !doc->Load(data)) {
        delete doc;
        return {};
    }
    return doc;
}

/* ********** PalmDOC (and TealDoc) ********** */

PalmDoc::PalmDoc(Str path) {
    this->fileName = str::Dup(path);
}

PalmDoc::~PalmDoc() {
    str::Free(fileName);
}

#define PDB_TOC_ENTRY_MARK "ToC!Entry!"

// http://wiki.mobileread.com/wiki/TealDoc
static Str HandleTealDocTag(str::Builder& builder, StrVec& tocEntries, Str text, int n, uint) {
    if (n < 9) {
    Fallback:
        builder.Append("&lt;");
        return text;
    }
    if (!str::StartsWithI(text, "<BOOKMARK") && !str::StartsWithI(text, "<HEADER") &&
        !str::StartsWithI(text, "<HRULE") && !str::StartsWithI(text, "<LABEL") && !str::StartsWithI(text, "<LINK") &&
        !str::StartsWithI(text, "<TEALPAINT")) {
        goto Fallback;
    }
    GumboHtmlParser parser(Str(text.s, n));
    HtmlToken* tok = parser.Next();
    if (!tok || !tok->IsStartTag()) {
        goto Fallback;
    }

    if (tok->NameIs(StrL("BOOKMARK"))) {
        // <BOOKMARK NAME="Contents">
        AttrInfo* attr = tok->GetAttrByName(StrL("NAME"));
        if (attr && attr->val) {
            TempStr s = strconv::HtmlUtf8ToStrTemp(attr->val);
            tocEntries.Append(s);
            builder.Append(fmt("<a name=" PDB_TOC_ENTRY_MARK "%d>", ::len(tocEntries)));
            return Str(tok->s.s + tok->s.len, (int)(text.s + text.len - (tok->s.s + tok->s.len)));
        }
    } else if (tok->NameIs(StrL("HEADER"))) {
        // <HEADER TEXT="Contents" ALIGN=CENTER STYLE=UNDERLINE>
        int hx = 2;
        AttrInfo* attr = tok->GetAttrByName(StrL("FONT"));
        if (attr && attr->val) {
            hx = '0' == attr->val.s[0] ? 5 : '2' == attr->val.s[0] ? 1 : 3;
        }
        attr = tok->GetAttrByName(StrL("TEXT"));
        if (attr) {
            builder.Append(fmt("<h%d>", hx));
            builder.Append(attr->val);
            builder.Append(fmt("</h%d>", hx));
            return Str(tok->s.s + tok->s.len, (int)(text.s + text.len - (tok->s.s + tok->s.len)));
        }
    } else if (tok->NameIs(StrL("HRULE"))) {
        // <HRULE STYLE=OUTLINE>
        builder.Append("<hr>");
        return Str(tok->s.s + tok->s.len, (int)(text.s + text.len - (tok->s.s + tok->s.len)));
    } else if (tok->NameIs(StrL("LABEL"))) {
        // <LABEL NAME="Contents">
        AttrInfo* attr = tok->GetAttrByName(StrL("NAME"));
        if (attr && attr->val) {
            builder.Append("<a name=\"");
            builder.Append(attr->val);
            builder.Append("\">");
            return Str(tok->s.s + tok->s.len, (int)(text.s + text.len - (tok->s.s + tok->s.len)));
        }
    } else if (tok->NameIs(StrL("LINK"))) {
        // <LINK TEXT="Press Me" TAG="Contents" FILE="My Novels">
        AttrInfo* attrTag = tok->GetAttrByName(StrL("TAG"));
        AttrInfo* attrText = tok->GetAttrByName(StrL("TEXT"));
        if (attrTag && attrText) {
            if (tok->GetAttrByName(StrL("FILE"))) {
                // skip links to other files
                return Str(tok->s.s + tok->s.len, (int)(text.s + text.len - (tok->s.s + tok->s.len)));
            }
            builder.Append("<a href=\"#");
            builder.Append(attrTag->val);
            builder.Append("\">");
            builder.Append(attrText->val);
            builder.Append("</a>");
            return Str(tok->s.s + tok->s.len, (int)(text.s + text.len - (tok->s.s + tok->s.len)));
        }
    } else if (tok->NameIs(StrL("TEALPAINT"))) {
        // <TEALPAINT SRC="Pictures" INDEX=0 LINK=SUPERMAP SUPERIMAGE=1 SUPERW=640 SUPERH=480>
        // support removed in r7047
        return Str(tok->s.s + tok->s.len, (int)(text.s + text.len - (tok->s.s + tok->s.len)));
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

    Str text = mobiDoc->GetHtmlData();
    uint codePage = GuessTextCodepage(text, CP_ACP);
    TempStr textUtf8 = strconv::ToMultiByteTemp(text, codePage, CP_UTF8);

    Str rest = textUtf8;
    // TODO: speedup by not calling htmlData.Append() for every byte
    // but gather spans and memcpy them wholesale
    for (int i = 0; i < rest.len; i++) {
        char c = rest.s[i];
        if ('&' == c) {
            htmlData.Append("&amp;");
        } else if ('<' == c) {
            Str after = HandleTealDocTag(htmlData, tocEntries, Str(rest.s + i, rest.len - i), rest.len - i, codePage);
            if (after) {
                i += (int)(after.s - (rest.s + i)) - 1;
            }
        } else if ('\n' == c || ('\r' == c && i + 1 < rest.len && '\n' != rest.s[i + 1])) {
            htmlData.Append("\n<br>");
        } else {
            htmlData.AppendChar(c);
        }
    }

    delete mobiDoc;
    return true;
}

Str PalmDoc::GetHtmlData() const {
    return ToStr(htmlData);
}

TempStr PalmDoc::GetPropertyTemp(DocProp) const {
    return {};
}

Str PalmDoc::GetFileName() const {
    return fileName;
}

bool PalmDoc::HasToc() const {
    return len(tocEntries) > 0;
}

bool PalmDoc::ParseToc(EbookTocVisitor* visitor) {
    for (int i = 0; i < len(tocEntries); i++) {
        TempStr url = fmt(PDB_TOC_ENTRY_MARK "%d", i + 1);
        Str name = tocEntries[i];
        visitor->Visit(name, url, 1);
    }
    return true;
}

bool PalmDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFilePalmDoc;
}

PalmDoc* PalmDoc::CreateFromFile(Str path) {
    PalmDoc* doc = new PalmDoc(path);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

/* ********** Plain HTML ********** */

HtmlDoc::HtmlDoc(Str path) : fileName(str::Dup(path)) {}

HtmlDoc::~HtmlDoc() {
    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName);
    }
    str::Free(htmlData);
    str::Free(fileName);
    str::Free(pagePath);
}

bool HtmlDoc::Load() {
    {
        Str data = file::ReadFile(fileName);
        if (!data) {
            return false;
        }
        TempStr decoded = DecodeTextToUtf8Temp(data, true);
        if (!decoded) {
            return false;
        }
        Str dup = str::Dup(decoded);
        htmlData = Str((char*)((u8*)dup.s), dup.len);
        str::Free(data);
    }

    str::ReplaceWithCopy(&pagePath, fileName);
    str::TransCharsInPlace(pagePath, StrL("\\"), StrL("/"));

    GumboHtmlParser parser(htmlData);
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsTag() || Tag_Body != tok->tag && Tag_P != tok->tag)) {
        if (tok->IsStartTag() && Tag_Title == tok->tag) {
            tok = parser.Next();
            if (tok && tok->IsText()) {
                TempStr val = ResolveHtmlEntitiesTemp(tok->s);
                AddProp(props, DocProp::Title, val);
            }
        } else if ((tok->IsStartTag() || tok->IsEmptyElementEndTag()) && Tag_Meta == tok->tag) {
            AttrInfo* attrName = tok->GetAttrByName(StrL("name"));
            AttrInfo* attrValue = tok->GetAttrByName(StrL("content"));
            if (!attrName || !attrValue) {
                /* ignore this tag */;
            } else if (attrName->ValIs("author")) {
                TempStr val = ResolveHtmlEntitiesTemp(attrValue->val);
                AddProp(props, DocProp::Author, val);
            } else if (attrName->ValIs("date")) {
                TempStr val = ResolveHtmlEntitiesTemp(attrValue->val);
                AddProp(props, DocProp::CreationDate, val);
            } else if (attrName->ValIs("copyright")) {
                TempStr val = ResolveHtmlEntitiesTemp(attrValue->val);
                AddProp(props, DocProp::Copyright, val);
            }
        }
    }

    return true;
}

Str HtmlDoc::GetHtmlData() {
    return htmlData;
}

Str HtmlDoc::GetImageData(Str fileName) {
    // TODO: this isn't thread-safe (might leak image data when called concurrently),

    TempStr url = NormalizeURLTemp(fileName, pagePath);
    for (int i = 0; i < len(images); i++) {
        if (str::Eq(images[i].fileName, url)) {
            return images[i].base;
        }
    }

    ImageData data;
    data.base = LoadURL(url);
    if (len(data.base) == 0) {
        return {};
    }
    data.fileName = str::Dup(url);
    images.Append(data);
    return images.Last().base;
}

Str HtmlDoc::GetFileData(Str relPath) {
    TempStr url = NormalizeURLTemp(relPath, pagePath);
    return LoadURL(url);
}

Str HtmlDoc::LoadURL(Str url) {
    if (str::StartsWith(url, "data:")) {
        return str::Dup(DecodeDataURITemp(url));
    }
    if (str::ContainsChar(url, ':')) {
        return {};
    }
    Str path = str::Dup(url);
    str::TransCharsInPlace(path, StrL("/"), StrL("\\"));
    return file::ReadFile(path);
}

TempStr HtmlDoc::GetPropertyTemp(DocProp prop) const {
    return GetPropValueTemp(props, prop);
}

Str HtmlDoc::GetFileName() const {
    return fileName;
}

bool HtmlDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFilePalmDoc;
}

HtmlDoc* HtmlDoc::CreateFromFile(Str fileName) {
    HtmlDoc* doc = new HtmlDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

/* ********** Plain Text (and RFCs and TCR) ********** */

TxtDoc::TxtDoc(Str fileName) {
    this->fileName = str::Dup(fileName);
}

TxtDoc::~TxtDoc() {
    str::Free(fileName);
}

// cf. http://www.cix.co.uk/~gidds/Software/TCR.html
#define TCR_HEADER "!!8-Bit!!"

static TempStr DecompressTcrTextTemp(Str data) {
    ReportIf(!str::StartsWith(data, TCR_HEADER));
    int hdrLen = LenL(TCR_HEADER);
    Str curr = Str(data.s + hdrLen, data.len - hdrLen);
    Str end = Str(data.s + data.len, 0);

    Str dict[256];
    for (int n = 0; n < (int)dimof(dict); n++) {
        if (!curr.len) {
            return str::DupTemp(data);
        }
        dict[n] = curr;
        int step = 1 + (u8)curr.s[0];
        curr = Str(curr.s + step, curr.len - step);
    }

    str::Builder text(data.len * 2);
    AtomicIntInc(&gAllowAllocFailure);
    AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);

    Str rest = curr;
    for (int i = 0; i < rest.len; i++) {
        Str entry = dict[(u8)rest.s[i]];
        bool ok = text.Append(Str(entry.s + 1, (u8)entry.s[0]));
        if (!ok) {
            return {};
        }
    }

    return ToStrTemp(text);
}

static Str TextFindLinkEnd(str::Builder& htmlData, Str curr, char prevChar, bool fromWww = false) {
    int endIdx = 0;
    while (endIdx < curr.len && !str::IsWs(curr.s[endIdx])) {
        endIdx++;
    }
    if (endIdx > 0) {
        char last = curr.s[endIdx - 1];
        if (',' == last || '.' == last || '?' == last || '!' == last) {
            endIdx--;
        }
    }
    if (endIdx > 0 && ')' == curr.s[endIdx - 1]) {
        int openParenIdx = str::IndexOfChar(curr, '(');
        if (openParenIdx < 0 || openParenIdx >= endIdx) {
            endIdx--;
        }
    }
    if (('"' == prevChar || '\'' == prevChar)) {
        int quoteIdx = str::IndexOfChar(curr, prevChar);
        if (quoteIdx >= 0 && quoteIdx < endIdx) {
            endIdx = quoteIdx;
        }
    }

    if (fromWww) {
        Str afterWww = Str(curr.s + 5, curr.len - 5);
        int dotIdx = str::IndexOfChar(afterWww, '.');
        if (endIdx <= 4 || dotIdx < 0 || dotIdx + 5 >= endIdx) {
            return {};
        }
    }

    htmlData.Append("<a href=\"");
    if (fromWww) {
        htmlData.Append("http://");
    }
    for (int i = 0; i < endIdx; i++) {
        AppendChar(htmlData, curr.s[i]);
    }
    htmlData.Append("\">");

    return Str(curr.s + endIdx, curr.len - endIdx);
}

// cf. http://weblogs.mozillazine.org/gerv/archives/2011/05/html5_email_address_regexp.html
inline bool IsEmailUsernameChar(char c) {
    // explicitly excluding the '/' from the list, as it is more
    // often part of a URL or path than of an email address
    return isalnum((u8)c) || str::ContainsChar(StrL(".!#$%&'*+=?^_`{|}~-"), c);
}
inline bool IsEmailDomainChar(char c) {
    return isalnum((u8)c) || '-' == c;
}

static Str TextFindEmailEnd(str::Builder& htmlData, Str curr) {
    Str beforeAt;
    Str rest = curr;
    if (len(curr) > 0 && '@' == curr.s[0]) {
        if (!IsEmailUsernameChar(htmlData.LastChar())) {
            return {};
        }
        int idx = len(htmlData);
        for (; idx > 1 && IsEmailUsernameChar(htmlData[idx - 1]); idx--) {
            ;
        }
        // copy (not a view): htmlData is mutated below before beforeAt is appended back
        beforeAt = str::DupTemp(Str(&htmlData[idx]));
    } else {
        ReportIf(!str::StartsWith(curr, "mailto:"));
        rest = Str(curr.s + 7, curr.len - 7);
        if (!rest.len || !IsEmailUsernameChar(rest.s[0])) {
            return {};
        }
        int i = 0;
        while (i < rest.len && IsEmailUsernameChar(rest.s[i])) {
            i++;
        }
        rest = Str(rest.s + i, rest.len - i);
    }

    if (!rest.len || rest.s[0] != '@' || rest.len < 2 || !IsEmailDomainChar(rest.s[1])) {
        return {};
    }
    int endIdx = 1;
    while (endIdx < rest.len && IsEmailDomainChar(rest.s[endIdx])) {
        endIdx++;
    }
    if (endIdx >= rest.len || '.' != rest.s[endIdx] || endIdx + 1 >= rest.len ||
        !IsEmailDomainChar(rest.s[endIdx + 1])) {
        return {};
    }
    do {
        endIdx++;
        while (endIdx < rest.len && IsEmailDomainChar(rest.s[endIdx])) {
            endIdx++;
        }
    } while (endIdx < rest.len && '.' == rest.s[endIdx] && endIdx + 1 < rest.len &&
             IsEmailDomainChar(rest.s[endIdx + 1]));

    Str end = Str(curr.s + (rest.s - curr.s) + endIdx, (int)(curr.len - (rest.s - curr.s) - endIdx));
    Str linkStart = len(curr) > 0 && '@' == curr.s[0] ? curr : Str(curr.s + 7, curr.len - 7);

    if (beforeAt) {
        int idx = len(htmlData) - beforeAt.len;
        htmlData.RemoveAt(idx, len(htmlData) - idx);
    }
    htmlData.Append("<a href=\"mailto:");
    htmlData.Append(beforeAt);
    for (int i = 0; i < (int)(end.s - linkStart.s); i++) {
        AppendChar(htmlData, linkStart.s[i]);
    }
    htmlData.Append("\">");
    htmlData.Append(beforeAt);

    return end;
}

static Str TextFindRfcEnd(str::Builder& htmlData, Str curr, char prevChar) {
    if (isalnum((u8)prevChar)) {
        return {};
    }
    int rfc;
    Str end = str::Parse(curr, "RFC %d", &rfc);
    if (str::IsNull(end)) {
        return {};
    }
    htmlData.Append(fmt("<a href='http://www.rfc-editor.org/rfc/rfc%d.txt'>", rfc));
    return end;
}

bool TxtDoc::Load() {
    Str fileContent = file::ReadFile(fileName);
    if (!fileContent) {
        return false;
    }

    TempStr text;
    Str raw = fileContent;
    if (str::EndsWithI(fileName, ".tcr") && str::StartsWith(raw, TCR_HEADER)) {
        text = DecompressTcrTextTemp(raw);
    } else {
        text = DecodeTextToUtf8Temp(raw);
    }
    if (!text) {
        return false;
    }

    int rfc;
    isRFC = !str::IsNull(str::Parse(path::GetBaseNameTemp(fileName), "rfc%d.txt%$", &rfc));

    int linkEndPos = -1;
    bool rfcHeader = false;
    int sectionCount = 0;

    htmlData.Append("<pre>");
    for (int i = 0; i < text.len; i++) {
        Str curr = Str(text.s + i, text.len - i);
        char c = text.s[i];
        // similar logic to LinkifyText in PdfEngine.cpp
        if (linkEndPos == i) {
            htmlData.Append("</a>");
            linkEndPos = -1;
        } else if (linkEndPos >= 0) {
            /* don't check for hyperlinks inside a link */;
        } else if ('@' == c) {
            Str end = TextFindEmailEnd(htmlData, curr);
            if (end) {
                linkEndPos = (int)(end.s - text.s);
            }
        } else if (i > 0 && ('/' == text.s[i - 1] || isalnum((u8)text.s[i - 1]))) {
            /* don't check for a link at this position */;
        } else if ('h' == c && !str::IsNull(str::Parse(curr, "http%?s://"))) {
            Str end = TextFindLinkEnd(htmlData, curr, i > 0 ? text.s[i - 1] : ' ');
            if (end) {
                linkEndPos = (int)(end.s - text.s);
            }
        } else if ('w' == c && str::StartsWith(curr, "www.")) {
            Str end = TextFindLinkEnd(htmlData, curr, i > 0 ? text.s[i - 1] : ' ', true);
            if (end) {
                linkEndPos = (int)(end.s - text.s);
            }
        } else if ('m' == c && str::StartsWith(curr, "mailto:")) {
            Str end = TextFindEmailEnd(htmlData, curr);
            if (end) {
                linkEndPos = (int)(end.s - text.s);
            }
        } else if (isRFC && i > 0 && 'R' == c && !str::IsNull(str::Parse(curr, "RFC %d", &rfc))) {
            Str end = TextFindRfcEnd(htmlData, curr, text.s[i - 1]);
            if (end) {
                linkEndPos = (int)(end.s - text.s);
            }
        }

        // RFCs use (among others) form feeds as page separators
        if ('\f' == c && (i == 0 || '\n' == text.s[i - 1]) &&
            (i + 1 >= text.len || '\r' == text.s[i + 1] || '\n' == text.s[i + 1])) {
            if (i > 0 && i + 2 < text.len && (i + 3 < text.len || text.s[i + 2] != '\n')) {
                htmlData.Append("<pagebreak />");
            }
            continue;
        }

        if (isRFC && i > 0 && '\n' == text.s[i - 1] && (str::IsDigit(c) || str::StartsWith(curr, "APPENDIX"))) {
            Str lineBefore, lineAfter;
            if (str::CutChar(curr, '\n', &lineBefore, &lineAfter) && !str::IsNull(str::Parse(lineAfter, "%?\r\n"))) {
                htmlData.Append(fmt("<b id='section%d' title=\"", ++sectionCount));
                for (int j = 0; j < lineBefore.len; j++) {
                    char ch = lineBefore.s[j];
                    if ('\r' == ch || '\n' == ch) {
                        break;
                    }
                    AppendChar(htmlData, ch);
                }
                htmlData.Append("\">");
                rfcHeader = true;
            }
        }
        if (rfcHeader && ('\r' == c || '\n' == c)) {
            htmlData.Append("</b>");
            rfcHeader = false;
        }

        AppendChar(htmlData, c);
    }
    if (linkEndPos >= 0) {
        htmlData.Append("</a>");
    }
    htmlData.Append("</pre>");

    return true;
}

Str TxtDoc::GetHtmlData() const {
    return ToStr(htmlData);
}

TempStr TxtDoc::GetPropertyTemp(DocProp) const {
    return {};
}

Str TxtDoc::GetFileName() const {
    return fileName;
}

bool TxtDoc::IsRFC() const {
    return isRFC;
}

bool TxtDoc::HasToc() const {
    return isRFC;
}

static inline Str SkipDigits(Str s) {
    while (s.len > 0 && str::IsDigit(s.s[0])) {
        s = Str(s.s + 1, s.len - 1);
    }
    return s;
}

bool TxtDoc::ParseToc(EbookTocVisitor* visitor) {
    if (!isRFC) {
        return false;
    }

    GumboDoc doc(ToStr(htmlData), false);
    Vec<const GumboNode*> toVisit;
    toVisit.Append(doc.Document());
    while (len(toVisit) > 0) {
        const GumboNode* node = toVisit.Pop();
        if (!node) {
            continue;
        }
        const GumboVector* children = GumboChildrenOf(node);
        if (children) {
            for (unsigned int i = children->length; i > 0; i--) {
                toVisit.Append((const GumboNode*)children->data[i - 1]);
            }
        }
        if (!GumboTagNameIs(node, StrL("b"))) {
            continue;
        }
        TempStr title = GumboAttributeValueTemp(node, "title");
        TempStr id = GumboAttributeValueTemp(node, "id");
        int level = 1;
        if (title && str::IsDigit(title.s[0])) {
            Str dot = SkipDigits(title);
            while (dot.len > 1 && dot.s[0] == '.' && str::IsDigit(dot.s[1])) {
                level++;
                dot = SkipDigits(Str(dot.s + 1, dot.len - 1));
            }
        }
        visitor->Visit(title, id, level);
    }

    return true;
}

bool TxtDoc::IsSupportedFileType(Kind kind) {
    return kind == kindFileTxt;
}

TxtDoc* TxtDoc::CreateFromFile(Str fileName) {
    TxtDoc* doc = new TxtDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}
