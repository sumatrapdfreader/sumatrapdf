/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Archive.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"

// tries to extract an encoding from <?xml encoding="..."?>
// returns CP_ACP on failure
static uint GetCodepageFromPI(Str xmlPI) {
    if (!str::StartsWith(xmlPI, "<?xml")) {
        return CP_ACP;
    }
    Str xmlPIEnd = str::Find(xmlPI, "?>");
    if (!xmlPIEnd) {
        return CP_ACP;
    }
    HtmlToken pi;
    pi.SetTag(HtmlToken::EmptyElementTag, xmlPI.s + 2, xmlPIEnd.s);
    pi.nLen = 4;
    AttrInfo* enc = pi.GetAttrByName("encoding");
    if (!enc) {
        return CP_ACP;
    }

    TempStr encoding = str::DupTemp(enc->val);
    struct {
        Str namePart;
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
        WCHAR* ws = str::CastToWCHAR(s.s);
        return ToUtf8Temp(ws);
    }
    if (str::StartsWith(s, UTF16BE_BOM)) {
        // convert from utf16 big endian to utf16
        s = Str(s.s + 2, s.len - 2);
        int n = str::Leni((WCHAR*)s.s);
        for (int i = 0; i < n; i++) {
            int idx = i * 2;
            std::swap(s.s[idx], s.s[idx + 1]);
        }
        WCHAR* ws = str::CastToWCHAR(s.s);
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

Str NormalizeURL(Str url, Str base) {
    ReportIf(!url || !base);
    if (url.s[0] == '/' || str::FindChar(url, ':')) {
        return str::Dup(url);
    }

    Str baseEnd = str::FindCharLast(base, '/');
    Str hash = str::FindChar(base, '#');
    int basePathLen;
    if (url.s[0] == '#') {
        basePathLen = hash ? (int)(hash.s - base.s) : base.len;
    } else if (baseEnd && hash && hash.s < baseEnd.s) {
        Str scan = Str(hash.s - 1, base.s + base.len - (hash.s - 1));
        while (scan.len > 0 && scan.s[0] != '/') {
            scan.s--;
            scan.len++;
        }
        basePathLen = scan.len > 0 ? (int)(scan.s - base.s + 1) : 0;
    } else if (baseEnd) {
        basePathLen = (int)(baseEnd.s - base.s + 1);
    } else {
        basePathLen = 0;
    }
    TempStr basePath = basePathLen > 0 ? str::DupTemp(Str(base.s, basePathLen)) : Str{};
    Str norm = str::Join(basePath, url);

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

static inline void AppendChar(StrBuilder& htmlData, char c) {
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
static ByteSlice DecodeDataURI(Str url) {
    Str comma = str::FindChar(url, ',');
    if (!comma) {
        return {};
    }
    Str data = Str(comma.s + 1, url.s + url.len - (comma.s + 1));
    if ((int)(comma.s - url.s) >= 12 && str::EqN(Str(comma.s - 7, 7), Str(";base64"), 7)) {
        ByteSlice d{(u8*)data.s, (size_t)data.len};
        return Base64Decode(d);
    }
    Str dup = str::Dup(data);
    return {(u8*)dup.s, (size_t)(unsigned)dup.len};
}

/* ********** EPUB ********** */

static Str EPUB_CONTAINER_NS() {
    return Str("urn:oasis:names:tc:opendocument:xmlns:container");
}
static Str EPUB_OPF_NS() {
    return Str("http://www.idpf.org/2007/opf");
}
static Str EPUB_NCX_NS() {
    return Str("http://www.daisy.org/z3986/2005/ncx/");
}
static Str EPUB_ENC_NS() {
    return Str("http://www.w3.org/2001/04/xmlenc#");
}

EpubDoc::EpubDoc(Str fileName) {
    this->fileName.SetCopy(fileName);
    InitializeCriticalSection(&zipAccess);
    archive = OpenArchiveFromFile(fileName, /*eagerLoad=*/true, gArchiveProgressCb);
}

EpubDoc::EpubDoc(IStream* stream) {
    InitializeCriticalSection(&zipAccess);
    archive = OpenArchiveFromStream(stream);
}

EpubDoc::~EpubDoc() {
    EnterCriticalSection(&zipAccess);

    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName.s);
    }

    LeaveCriticalSection(&zipAccess);
    DeleteCriticalSection(&zipAccess);
    delete archive;
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

bool EpubDoc::Load() {
    if (!archive) {
        return false;
    }
    auto* containerFi = archive->GetFileDataByName("META-INF/container.xml");
    if (!containerFi || !containerFi->data) {
        return false;
    }
    ByteSlice container{(u8*)containerFi->data, containerFi->fileSizeUncompressed};
    HtmlParser parser;
    HtmlElement* node = parser.ParseInPlace(container);
    if (!node) {
        return false;
    }

    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByNameNS("rootfile", EPUB_CONTAINER_NS());
    if (!node) {
        return false;
    }
    TempStr contentPath = node->GetAttributeTemp("full-path");
    if (!contentPath) {
        return false;
    }
    url::DecodeInPlace(contentPath);

    // encrypted files will be ignored (TODO: support decryption)
    StrVec encList;
    auto* encryptionFi = archive->GetFileDataByName("META-INF/encryption.xml");
    if (encryptionFi && encryptionFi->data) {
        ByteSlice encryption{(u8*)encryptionFi->data, encryptionFi->fileSizeUncompressed};
        (void)parser.ParseInPlace(encryption);
        HtmlElement* cr = parser.FindElementByNameNS("CipherReference", EPUB_ENC_NS());
        while (cr) {
            WStr uriW = cr->GetAttribute("URI");
            if (uriW) {
                TempStr uri = ToUtf8Temp(uriW);
                url::DecodeInPlace(uri);
                encList.Append(uri);
                str::Free(uriW.s);
            }
            cr = parser.FindElementByNameNS("CipherReference", EPUB_ENC_NS(), cr);
        }
    }

    auto* contentFi = archive->GetFileDataByName(contentPath);
    if (!contentFi || !contentFi->data) {
        return false;
    }
    ByteSlice content{(u8*)contentFi->data, contentFi->fileSizeUncompressed};
    ParseMetadata(AsStr(content));
    node = parser.ParseInPlace(content);
    if (!node) {
        return false;
    }
    node = parser.FindElementByNameNS("manifest", EPUB_OPF_NS());
    if (!node) {
        return false;
    }

    Str slashPos = str::FindCharLast(contentPath, '/');
    if (slashPos) {
        contentPath = str::DupTemp(Str(contentPath.s, (int)(slashPos.s - contentPath.s + 1)));
    } else {
        contentPath = {};
    }

    StrVec idList, pathList;

    for (node = node->down; node; node = node->next) {
        TempStr mediaType = node->GetAttributeTemp("media-type");
        if (isImageMediaType(mediaType)) {
            TempStr imgPath = node->GetAttributeTemp("href");
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
            data.fileName = str::Dup(Str(imgPath));
            data.fileId = archive->GetFileId(data.fileName);
            images.Append(data);
        } else if (isHtmlMediaType(mediaType)) {
            TempStr htmlPath = node->GetAttributeTemp("href");
            if (!htmlPath) {
                continue;
            }
            url::DecodeInPlace(htmlPath);
            TempStr htmlId = node->GetAttributeTemp("id");
            // EPUB 3 ToC
            TempStr properties = node->GetAttributeTemp("properties");
            if (properties && str::Find(properties, "nav") && str::Eq(mediaType, "application/xhtml+xml")) {
                tocPath.Set(str::Join(contentPath, htmlPath));
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

    node = parser.FindElementByNameNS("spine", EPUB_OPF_NS());
    if (!node) {
        return false;
    }

    // EPUB 2 ToC
    TempStr tocId = node->GetAttributeTemp("toc");
    if (tocId && !tocPath && idList.Contains(tocId)) {
        auto idx = idList.Find(tocId);
        auto s = pathList.At(idx);
        tocPath.Set(str::Join(contentPath, s));
        isNcxToc = true;
    }
    AutoFreeWStr readingDir(node->GetAttribute(Str("page-progression-direction")).s);
    if (readingDir) {
        isRtlDoc = str::EqI(readingDir, L"rtl");
    }

    for (node = node->down; node; node = node->next) {
        if (!node->NameIsNS("itemref", EPUB_OPF_NS())) {
            continue;
        }
        TempStr idref = node->GetAttributeTemp("idref");
        if (!idref || !idList.Contains(idref)) {
            continue;
        }

        auto idx = idList.Find(idref);
        Str fname = pathList.At(idx);
        TempStr fullPath = str::JoinTemp(contentPath, fname);
        auto* htmlFi = archive->GetFileDataByName(fullPath);
        if (!htmlFi || !htmlFi->data) {
            continue;
        }
        ByteSlice html{(u8*)htmlFi->data, htmlFi->fileSizeUncompressed};
        TempStr decoded = DecodeTextToUtf8Temp(AsStr(html), true);
        if (!decoded) {
            continue;
        }
        // insert explicit page-breaks between sections including
        // an anchor with the file name at the top (for internal links)
        ReportIf(str::FindChar(fullPath, '"'));
        str::TransCharsInPlace(fullPath, "\"", "'");
        htmlData.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", fullPath);
        htmlData.Append(decoded);
    }

    return htmlData.size() > 0;
}

// clang-format off
static Str epubPropsMap[] = {
    kPropTitle, "dc:title",
    kPropAuthor, "dc:creator",
    kPropCreationDate, "dc:date",
    kPropModificationDate, "dcterms:modified",
    kPropSubject, "dc:description",
    kPropCopyright, "dc:rights",
};
// clang-format on

static bool IsTokPropName(HtmlToken* tok, Str name) {
    if (tok->NameIs(name)) {
        return true;
    }
    if (Tag_Meta != tok->tag) {
        return false;
    }
    AttrInfo* attr = tok->GetAttrByName("property");
    return attr && attr->ValIs(name);
}

void EpubDoc::ParseMetadata(Str content) {
    HtmlPullParser pullParser(content.s, content.len);
    int insideMetadata = 0;
    HtmlToken* tok;

    while ((tok = pullParser.Next()) != nullptr) {
        if (tok->IsStartTag() && tok->NameIsNS("metadata", EPUB_OPF_NS())) {
            insideMetadata++;
        } else if (tok->IsEndTag() && tok->NameIsNS("metadata", EPUB_OPF_NS())) {
            insideMetadata--;
        }
        if (!insideMetadata) {
            continue;
        }
        if (!tok->IsStartTag()) {
            continue;
        }

        int nProps = dimofi(epubPropsMap) / 2;
        for (int i = 0; i < nProps; i++) {
            int idx = i * 2;
            Str epubName = epubPropsMap[idx + 1];
            // TODO: implement proper namespace support
            if (!IsTokPropName(tok, epubName)) {
                continue;
            }
            tok = pullParser.Next();
            if (tok && tok->IsText()) {
                auto prop = epubPropsMap[idx];
                TempStr val = ResolveHtmlEntitiesTemp(Str(tok->s, tok->sLen));
                AddProp(props, prop, val);
            }
            break;
        }
    }
}

ByteSlice EpubDoc::GetHtmlData() const {
    return htmlData.AsByteSlice();
}

ByteSlice* EpubDoc::GetImageData(Str fileName, Str pagePath) {
    ScopedCritSec scope(&zipAccess);

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
                if (img.base.empty()) {
                    auto* fi = archive->GetFileDataById(img.fileId);
                    if (fi && fi->data) {
                        img.base = {(u8*)fi->data, fi->fileSizeUncompressed};
                        fi->data = nullptr;
                    }
                }
                if (!img.base.empty()) {
                    return &img.base;
                }
            }
        }
        return {};
    }

    AutoFreeStr url(NormalizeURL(fileName, Str(pagePath)).s);
    // some EPUB producers use wrong path separators
    if (str::FindChar(url, '\\')) {
        str::TransCharsInPlace(Str(url), "\\", "/");
    }
    for (ImageData& img : images) {
        if (str::Eq(img.fileName, url)) {
            if (img.base.empty()) {
                auto* fi = archive->GetFileDataById(img.fileId);
                if (fi && fi->data) {
                    img.base = {(u8*)fi->data, fi->fileSizeUncompressed};
                    fi->data = nullptr;
                }
            }
            if (!img.base.empty()) {
                return &img.base;
            }
        }
    }

    // try to also load images which aren't registered in the manifest
    ImageData data;
    data.fileId = archive->GetFileId(Str(url));
    if (data.fileId != (size_t)-1) {
        auto* fi = archive->GetFileDataById(data.fileId);
        if (fi && fi->data) {
            data.base = {(u8*)fi->data, fi->fileSizeUncompressed};
            fi->data = nullptr;
            data.fileName = str::Dup(Str(url));
            images.Append(data);
            return &images.Last().base;
        }
    }

    return {};
}

ByteSlice EpubDoc::GetFileData(Str relPath, Str pagePath) {
    if (!pagePath) {
        ReportIf(true);
        return {};
    }

    ScopedCritSec scope(&zipAccess);

    AutoFreeStr url(NormalizeURL(relPath, Str(pagePath)).s);
    auto* fi = archive->GetFileDataByName(Str(url));
    if (!fi || !fi->data) {
        return {};
    }
    ByteSlice res{(u8*)fi->data, fi->fileSizeUncompressed};
    fi->data = nullptr;
    return res;
}

TempStr EpubDoc::GetPropertyTemp(Str name) const {
    return GetPropValueTemp(props, name);
}

Str EpubDoc::GetFileName() const {
    return Str(fileName);
}

bool EpubDoc::IsRTL() const {
    return isRtlDoc;
}

bool EpubDoc::HasToc() const {
    return tocPath != nullptr;
}

bool EpubDoc::ParseNavToc(Str data, Str pagePath, EbookTocVisitor* visitor) {
    HtmlPullParser parser(data.s, data.len);
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
                    href.SetCopy(attrInfo->val);
                }
            }
            while ((tok = parser.Next()) != nullptr && !tok->IsError() && (!tok->IsEndTag() || itemTag != tok->tag)) {
                if (tok->IsText()) {
                    AutoFreeStr part = str::Dup(tok->s, tok->sLen).s;
                    if (!text) {
                        text.Set(part.Release());
                    } else {
                        text.Set(str::Join(Str(text.Get()), Str(part.Get())).s);
                    }
                }
            }
            if (!text) {
                continue;
            }
            auto itemText = ToWStrTemp(text.Get());
            str::NormalizeWSInPlace(itemText);
            AutoFreeWStr itemSrc;
            if (href) {
                href.Set(NormalizeURL(Str(href), Str(pagePath)).s);
                itemSrc.Set(strconv::FromHtmlUtf8(Str(href.Get())));
            }
            TempStr txt = ToUtf8Temp(itemText);
            TempStr src = ToUtf8Temp(itemSrc);
            visitor->Visit(txt, src, level);
        }
    }

    return true;
}

bool EpubDoc::ParseNcxToc(Str data, Str pagePath, EbookTocVisitor* visitor) {
    HtmlPullParser parser(data.s, data.len);
    HtmlToken* tok;
    // skip to the start of the navMap
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && tok->NameIsNS("navMap", EPUB_NCX_NS())) {
            break;
        }
    }
    if (!tok || tok->IsError()) {
        return false;
    }

    AutoFreeWStr itemText, itemSrc;
    int level = 0;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsEndTag() || !tok->NameIsNS("navMap", EPUB_NCX_NS()))) {
        if (tok->IsTag() && tok->NameIsNS("navPoint", EPUB_NCX_NS())) {
            if (itemText) {
                TempStr txt = ToUtf8Temp(itemText);
                TempStr src = ToUtf8Temp(itemSrc);
                visitor->Visit(txt, src, level);
                itemText.Reset();
                itemSrc.Reset();
            }
            if (tok->IsStartTag()) {
                level++;
            } else if (tok->IsEndTag() && level > 0) {
                level--;
            }
        } else if (tok->IsStartTag() && tok->NameIsNS("text", EPUB_NCX_NS())) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                itemText.Set(strconv::FromHtmlUtf8(Str(tok->s, tok->sLen)));
            }
        } else if (tok->IsTag() && !tok->IsEndTag() && tok->NameIsNS("content", EPUB_NCX_NS())) {
            AttrInfo* attrInfo = tok->GetAttrByName("src");
            if (attrInfo) {
                AutoFreeStr src = str::Dup(attrInfo->val).s;
                src.Set(NormalizeURL(Str(src), Str(pagePath)).s);
                itemSrc.Set(strconv::FromHtmlUtf8(Str(src)));
            }
        }
    }

    return true;
}

bool EpubDoc::ParseToc(EbookTocVisitor* visitor) {
    if (!tocPath) {
        return false;
    }
    Str tocDataStr;
    {
        ScopedCritSec scope(&zipAccess);
        auto* fi = archive->GetFileDataByName(Str(tocPath));
        if (fi && fi->data) {
            tocDataStr = Str(fi->data, (int)fi->fileSizeUncompressed);
        }
    }
    if (!tocDataStr) {
        return false;
    }

    Str pagePath(tocPath.Get());
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

EpubDoc* EpubDoc::CreateFromStream(IStream* stream) {
    EpubDoc* doc = new EpubDoc(stream);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

/* ********** FictionBook (FB2) ********** */

static Str FB2_MAIN_NS() {
    return Str("http://www.gribuser.ru/xml/fictionbook/2.0");
}
static Str FB2_XLINK_NS() {
    return Str("http://www.w3.org/1999/xlink");
}

Fb2Doc::Fb2Doc(Str fileName) : fileName(str::Dup(fileName).s) {}

Fb2Doc::Fb2Doc(IStream* stream) : stream(stream) {
    stream->AddRef();
}

Fb2Doc::~Fb2Doc() {
    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName.s);
    }
    if (stream) {
        stream->Release();
    }
}

static ByteSlice takeFileData(MultiFormatArchive* archive, size_t fileId) {
    auto* fi = archive->GetFileDataById(fileId);
    if (!fi || !fi->data) {
        return {};
    }
    ByteSlice res{(u8*)fi->data, fi->fileSizeUncompressed};
    fi->data = nullptr;
    return res;
}

static ByteSlice loadFromFile(Fb2Doc* doc) {
    MultiFormatArchive* archive = OpenArchiveFromFile(Str(doc->fileName), /*eagerLoad=*/true, gArchiveProgressCb);
    if (!archive) {
        return file::ReadFile(Str(doc->fileName));
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
        return takeFileData(archive, 0);
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
            data = takeFileData(archive, fileInfo->fileId);
        } else if (!str::EndsWithI(path, ".url")) {
            return {};
        }
    }
    return data;
}

static ByteSlice loadFromStream(Fb2Doc* doc) {
    auto stream = doc->stream;
    MultiFormatArchive* archive = OpenArchiveFromStream(stream);
    if (!archive) {
        return {};
    }

    AutoDelete delArchive(archive);
    size_t nFiles = archive->GetFileInfos().size();
    if (nFiles != 1) {
        return {};
    }
    doc->isZipped = true;
    return takeFileData(archive, 0);
}

bool Fb2Doc::Load() {
    ReportIf(!stream && !fileName);

    ByteSlice data;
    if (fileName) {
        data = loadFromFile(this);
    } else if (stream) {
        data = loadFromStream(this);
    }
    if (data.empty()) {
        return false;
    }
    TempStr tmp = DecodeTextToUtf8Temp(AsStr(data), true);
    data.Free();
    if (!tmp) {
        return false;
    }

    ByteSlice data2{(u8*)tmp.s, (size_t)tmp.len};

    HtmlPullParser parser(data2);
    HtmlToken* tok;
    int inBody = 0, inTitleInfo = 0, inDocInfo = 0;
    Str bodyStart;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!inTitleInfo && !inDocInfo && tok->IsStartTag() && Tag_Body == tok->tag) {
            if (!inBody++) {
                bodyStart = Str(tok->s);
            }
        } else if (inBody && tok->IsEndTag() && Tag_Body == tok->tag) {
            if (!--inBody) {
                if (xmlData.size() > 0) {
                    xmlData.Append("<pagebreak />");
                }
                xmlData.AppendChar('<');
                xmlData.Append(bodyStart.s, (int)(tok->s - bodyStart.s) + tok->sLen);
                xmlData.AppendChar('>');
            }
        } else if (inBody && tok->IsStartTag() && Tag_Title == tok->tag) {
            hasToc = true;
        } else if (inBody) {
            continue;
        } else if (inTitleInfo && tok->IsEndTag() && tok->NameIsNS("title-info", FB2_MAIN_NS())) {
            inTitleInfo--;
        } else if (inDocInfo && tok->IsEndTag() && tok->NameIsNS("document-info", FB2_MAIN_NS())) {
            inDocInfo--;
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("book-title", FB2_MAIN_NS())) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                TempStr val = ResolveHtmlEntitiesTemp(Str(tok->s, tok->sLen));
                AddProp(props, kPropTitle, val);
            }
        } else if ((inTitleInfo || inDocInfo) && tok->IsStartTag() && tok->NameIsNS("author", FB2_MAIN_NS())) {
            TempStr docAuthor = nullptr;
            while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
                   !(tok->IsEndTag() && tok->NameIsNS("author", FB2_MAIN_NS()))) {
                if (tok->IsText()) {
                    TempStr author = ResolveHtmlEntitiesTemp(Str(tok->s, tok->sLen));
                    if (docAuthor) {
                        docAuthor = str::JoinTemp(Str(docAuthor), Str(" "), Str(author));
                    } else {
                        docAuthor = author;
                    }
                }
            }
            if (docAuthor) {
                str::NormalizeWSInPlace(docAuthor);
                if (!str::IsEmpty(docAuthor)) {
                    TempStr val = docAuthor;
                    bool replaceIfExists = inTitleInfo != 0;
                    AddProp(props, kPropAuthor, val, replaceIfExists);
                }
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("date", FB2_MAIN_NS())) {
            AttrInfo* attr = tok->GetAttrByNameNS("value", FB2_MAIN_NS());
            if (attr) {
                TempStr val = ResolveHtmlEntitiesTemp(attr->val);
                AddProp(props, kPropCreationDate, val);
            }
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS("date", FB2_MAIN_NS())) {
            AttrInfo* attr = tok->GetAttrByNameNS("value", FB2_MAIN_NS());
            if (attr) {
                TempStr val = ResolveHtmlEntitiesTemp(attr->val);
                AddProp(props, kPropModificationDate, val);
            }
        } else if (inDocInfo && tok->IsStartTag() && tok->NameIsNS("program-used", FB2_MAIN_NS())) {
            if ((tok = parser.Next()) == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                TempStr val = ResolveHtmlEntitiesTemp(Str(tok->s, tok->sLen));
                AddProp(props, kPropCreatorApp, val);
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIsNS("coverpage", FB2_MAIN_NS())) {
            tok = parser.Next();
            if (tok && tok->IsText()) {
                tok = parser.Next();
            }
            if (tok && tok->IsEmptyElementEndTag() && Tag_Image == tok->tag) {
                AttrInfo* attr = tok->GetAttrByNameNS("href", FB2_XLINK_NS());
                if (attr) {
                    coverImage.SetCopy(attr->val);
                }
            }
        } else if (inTitleInfo || inDocInfo) {
            continue;
        } else if (tok->IsStartTag() && tok->NameIsNS("title-info", FB2_MAIN_NS())) {
            inTitleInfo++;
        } else if (tok->IsStartTag() && tok->NameIsNS("document-info", FB2_MAIN_NS())) {
            inDocInfo++;
        } else if (tok->IsStartTag() && tok->NameIsNS("binary", FB2_MAIN_NS())) {
            ExtractImage(&parser, tok);
        }
    }

    return xmlData.size() > 0;
}

void Fb2Doc::ExtractImage(HtmlPullParser* parser, HtmlToken* tok) {
    AutoFreeStr id;
    AttrInfo* attrInfo = tok->GetAttrByNameNS("id", FB2_MAIN_NS());
    if (attrInfo) {
        id.SetCopy(attrInfo->val);
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
    data.fileName = str::Join(Str("#"), Str(id));
    data.fileId = images.size();
    images.Append(data);
}

ByteSlice Fb2Doc::GetXmlData() const {
    return {(u8*)xmlData.Get(), xmlData.size()};
}

ByteSlice* Fb2Doc::GetImageData(Str fileName) const {
    for (size_t i = 0; i < images.size(); i++) {
        if (str::Eq(images.at(i).fileName, fileName)) {
            return &images.at(i).base;
        }
    }
    return {};
}

ByteSlice* Fb2Doc::GetCoverImage() const {
    if (!coverImage) {
        return {};
    }
    return GetImageData(Str(coverImage));
}

TempStr Fb2Doc::GetPropertyTemp(Str name) const {
    return GetPropValueTemp(props, name);
}

Str Fb2Doc::GetFileName() const {
    return Str(fileName);
}

bool Fb2Doc::IsZipped() const {
    return isZipped;
}

bool Fb2Doc::HasToc() const {
    return hasToc;
}

bool Fb2Doc::ParseToc(EbookTocVisitor* visitor) const {
    AutoFreeWStr itemText;
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
                TempStr txt = ToUtf8Temp(itemText);
                visitor->Visit(txt, url, level);
                itemText.Reset();
            }
            inTitle = false;
        } else if (inTitle && tok->IsText()) {
            AutoFreeWStr text(strconv::FromHtmlUtf8(Str(tok->s, tok->sLen)).s);
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

Fb2Doc* Fb2Doc::CreateFromFile(Str path) {
    Fb2Doc* doc = new Fb2Doc(path);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

Fb2Doc* Fb2Doc::CreateFromStream(IStream* stream) {
    Fb2Doc* doc = new Fb2Doc(stream);
    if (!doc || !doc->Load()) {
        delete doc;
        return {};
    }
    return doc;
}

/* ********** PalmDOC (and TealDoc) ********** */

PalmDoc::PalmDoc(Str path) {
    this->fileName = str::Dup(path).s;
}

PalmDoc::~PalmDoc() {}

#define PDB_TOC_ENTRY_MARK "ToC!Entry!"

// http://wiki.mobileread.com/wiki/TealDoc
static Str HandleTealDocTag(StrBuilder& builder, StrVec& tocEntries, Str text, size_t len, uint) {
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
        if (attr && attr->val) {
            WCHAR* ws = strconv::FromHtmlUtf8(attr->val);
            TempStr s = ToUtf8Temp(ws);
            tocEntries.Append(s);
            str::Free(ws);
            builder.AppendFmt("<a name=" PDB_TOC_ENTRY_MARK "%d>", tocEntries.Size());
            return Str(tok->s + tok->sLen, text.s + text.len - (tok->s + tok->sLen));
        }
    } else if (tok->NameIs("HEADER")) {
        // <HEADER TEXT="Contents" ALIGN=CENTER STYLE=UNDERLINE>
        int hx = 2;
        AttrInfo* attr = tok->GetAttrByName("FONT");
        if (attr && attr->val) {
            hx = '0' == attr->val.s[0] ? 5 : '2' == attr->val.s[0] ? 1 : 3;
        }
        attr = tok->GetAttrByName("TEXT");
        if (attr) {
            builder.AppendFmt("<h%d>", hx);
            builder.Append(attr->val);
            builder.AppendFmt("</h%d>", hx);
            return Str(tok->s + tok->sLen, text.s + text.len - (tok->s + tok->sLen));
        }
    } else if (tok->NameIs("HRULE")) {
        // <HRULE STYLE=OUTLINE>
        builder.Append("<hr>");
        return Str(tok->s + tok->sLen, text.s + text.len - (tok->s + tok->sLen));
    } else if (tok->NameIs("LABEL")) {
        // <LABEL NAME="Contents">
        AttrInfo* attr = tok->GetAttrByName("NAME");
        if (attr && attr->val) {
            builder.Append("<a name=\"");
            builder.Append(attr->val);
            builder.Append("\">");
            return Str(tok->s + tok->sLen, text.s + text.len - (tok->s + tok->sLen));
        }
    } else if (tok->NameIs("LINK")) {
        // <LINK TEXT="Press Me" TAG="Contents" FILE="My Novels">
        AttrInfo* attrTag = tok->GetAttrByName("TAG");
        AttrInfo* attrText = tok->GetAttrByName("TEXT");
        if (attrTag && attrText) {
            if (tok->GetAttrByName("FILE")) {
                // skip links to other files
                return Str(tok->s + tok->sLen, text.s + text.len - (tok->s + tok->sLen));
            }
            builder.Append("<a href=\"#");
            builder.Append(attrTag->val);
            builder.Append("\">");
            builder.Append(attrText->val);
            builder.Append("</a>");
            return Str(tok->s + tok->sLen, text.s + text.len - (tok->s + tok->sLen));
        }
    } else if (tok->NameIs("TEALPAINT")) {
        // <TEALPAINT SRC="Pictures" INDEX=0 LINK=SUPERMAP SUPERIMAGE=1 SUPERW=640 SUPERH=480>
        // support removed in r7047
        return Str(tok->s + tok->sLen, text.s + text.len - (tok->s + tok->sLen));
    }
    goto Fallback;
}

bool PalmDoc::Load() {
    MobiDoc* mobiDoc = MobiDoc::CreateFromFile(Str(fileName.Get()));
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
    uint codePage = GuessTextCodepage(AsStr(text), CP_ACP);
    TempStr textUtf8 = strconv::ToMultiByteTemp(AsStr(text), codePage, CP_UTF8);

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

ByteSlice PalmDoc::GetHtmlData() const {
    return htmlData.AsByteSlice();
}

TempStr PalmDoc::GetPropertyTemp(Str) const {
    return {};
}

Str PalmDoc::GetFileName() const {
    return Str(fileName);
}

bool PalmDoc::HasToc() const {
    return tocEntries.Size() > 0;
}

bool PalmDoc::ParseToc(EbookTocVisitor* visitor) {
    for (int i = 0; i < tocEntries.Size(); i++) {
        TempStr url = str::FormatTemp(PDB_TOC_ENTRY_MARK "%d", i + 1);
        Str name = tocEntries.At(i);
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

HtmlDoc::HtmlDoc(Str path) : fileName(str::Dup(path).s) {}

HtmlDoc::~HtmlDoc() {
    for (auto&& img : images) {
        str::Free(img.base);
        str::Free(img.fileName.s);
    }
    htmlData.Free();
}

bool HtmlDoc::Load() {
    {
        ByteSlice data = file::ReadFile(Str(fileName));
        if (!data) {
            return false;
        }
        TempStr decoded = DecodeTextToUtf8Temp(AsStr(data), true);
        if (!decoded) {
            return false;
        }
        htmlData = str::Dup(decoded).s;
        data.Free();
    }

    pagePath.SetCopy(Str(fileName));
    str::TransCharsInPlace(Str(pagePath), "\\", "/");

    HtmlPullParser parser(htmlData);
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsTag() || Tag_Body != tok->tag && Tag_P != tok->tag)) {
        if (tok->IsStartTag() && Tag_Title == tok->tag) {
            tok = parser.Next();
            if (tok && tok->IsText()) {
                TempStr val = ResolveHtmlEntitiesTemp(Str(tok->s, tok->sLen));
                AddProp(props, kPropTitle, val);
            }
        } else if ((tok->IsStartTag() || tok->IsEmptyElementEndTag()) && Tag_Meta == tok->tag) {
            AttrInfo* attrName = tok->GetAttrByName("name");
            AttrInfo* attrValue = tok->GetAttrByName("content");
            if (!attrName || !attrValue) {
                /* ignore this tag */;
            } else if (attrName->ValIs("author")) {
                TempStr val = ResolveHtmlEntitiesTemp(attrValue->val);
                AddProp(props, kPropAuthor, val);
            } else if (attrName->ValIs("date")) {
                TempStr val = ResolveHtmlEntitiesTemp(attrValue->val);
                AddProp(props, kPropCreationDate, val);
            } else if (attrName->ValIs("copyright")) {
                TempStr val = ResolveHtmlEntitiesTemp(attrValue->val);
                AddProp(props, kPropCopyright, val);
            }
        }
    }

    return true;
}

ByteSlice HtmlDoc::GetHtmlData() {
    return htmlData;
}

ByteSlice* HtmlDoc::GetImageData(Str fileName) {
    // TODO: this isn't thread-safe (might leak image data when called concurrently),

    AutoFreeStr url(NormalizeURL(fileName, Str(pagePath)).s);
    for (size_t i = 0; i < images.size(); i++) {
        if (str::Eq(images.at(i).fileName, url)) {
            return &images.at(i).base;
        }
    }

    ImageData data;
    data.base = LoadURL(Str(url));
    if (data.base.empty()) {
        return {};
    }
    data.fileName = Str(url.Release());
    images.Append(data);
    return &images.Last().base;
}

ByteSlice HtmlDoc::GetFileData(Str relPath) {
    AutoFreeStr url(NormalizeURL(relPath, Str(pagePath)).s);
    return LoadURL(Str(url));
}

ByteSlice HtmlDoc::LoadURL(Str url) {
    if (str::StartsWith(url, "data:")) {
        auto res = DecodeDataURI(url);
        return res;
    }
    if (str::FindChar(url, ':')) {
        return {};
    }
    Str path = str::Dup(url);
    str::TransCharsInPlace(path, "/", "\\");
    return file::ReadFile(path);
}

TempStr HtmlDoc::GetPropertyTemp(Str name) const {
    return GetPropValueTemp(props, name);
}

Str HtmlDoc::GetFileName() const {
    return Str(fileName);
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
    this->fileName = str::Dup(fileName).s;
}

// cf. http://www.cix.co.uk/~gidds/Software/TCR.html
#define TCR_HEADER "!!8-Bit!!"

static TempStr DecompressTcrTextTemp(Str data) {
    ReportIf(!str::StartsWith(data, TCR_HEADER));
    int hdrLen = str::Len(TCR_HEADER);
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

    StrBuilder text(data.len * 2);
    InterlockedIncrement(&gAllowAllocFailure);
    defer {
        InterlockedDecrement(&gAllowAllocFailure);
    };

    Str rest = curr;
    for (int i = 0; i < rest.len; i++) {
        Str entry = dict[(u8)rest.s[i]];
        bool ok = text.Append(entry.s + 1, (u8)entry.s[0]);
        if (!ok) {
            return {};
        }
    }

    return text.StealData(GetTempArena());
}

static Str TextFindLinkEnd(StrBuilder& htmlData, Str curr, char prevChar, bool fromWww = false) {
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
        Str openParen = str::FindChar(curr, '(');
        if (!openParen || openParen.s >= curr.s + endIdx) {
            endIdx--;
        }
    }
    if (('"' == prevChar || '\'' == prevChar)) {
        Str quote = str::FindChar(curr, prevChar);
        if (quote && quote.s < curr.s + endIdx) {
            endIdx = (int)(quote.s - curr.s);
        }
    }

    if (fromWww && (endIdx <= 4 || !str::FindChar(Str(curr.s + 5, curr.len - 5), '.') ||
                    str::FindChar(Str(curr.s + 5, curr.len - 5), '.').s >= curr.s + endIdx)) {
        return {};
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
    return isalnum((u8)c) || c && str::FindChar(".!#$%&'*+=?^_`{|}~-", c);
}
inline bool IsEmailDomainChar(char c) {
    return isalnum((u8)c) || '-' == c;
}

static Str TextFindEmailEnd(StrBuilder& htmlData, Str curr) {
    AutoFreeStr beforeAt;
    Str rest = curr;
    if (curr.len > 0 && '@' == curr.s[0]) {
        if (htmlData.size() == 0 || !IsEmailUsernameChar(htmlData.Last())) {
            return {};
        }
        size_t idx = htmlData.size();
        for (; idx > 1 && IsEmailUsernameChar(htmlData.at(idx - 1)); idx--) {
            ;
        }
        beforeAt.SetCopy(&htmlData.at(idx));
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

    Str end = Str(curr.s + (rest.s - curr.s) + endIdx, curr.len - (rest.s - curr.s) - endIdx);
    Str linkStart = curr.len > 0 && '@' == curr.s[0] ? curr : Str(curr.s + 7, curr.len - 7);

    if (beforeAt) {
        size_t idx = htmlData.size() - str::Len(beforeAt);
        htmlData.RemoveAt(idx, htmlData.size() - idx);
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

static Str TextFindRfcEnd(StrBuilder& htmlData, Str curr, char prevChar) {
    if (isalnum((u8)prevChar)) {
        return {};
    }
    int rfc;
    Str end = str::Parse(curr, "RFC %d", &rfc);
    if (!end) {
        return {};
    }
    htmlData.AppendFmt("<a href='http://www.rfc-editor.org/rfc/rfc%d.txt'>", rfc);
    return end;
}

bool TxtDoc::Load() {
    ByteSlice fileContent = file::ReadFile(Str(fileName));
    if (!fileContent) {
        return false;
    }

    TempStr text;
    Str raw = AsStr(fileContent);
    if (str::EndsWithI(fileName, ".tcr") && str::StartsWith(raw, TCR_HEADER)) {
        text = DecompressTcrTextTemp(raw);
    } else {
        text = DecodeTextToUtf8Temp(raw);
    }
    if (!text) {
        return false;
    }

    int rfc;
    isRFC = str::Parse(path::GetBaseNameTemp(Str(fileName)), "rfc%d.txt%$", &rfc) != nullptr;

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
        } else if ('h' == c && str::Parse(curr, "http%?s://")) {
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
        } else if (isRFC && i > 0 && 'R' == c && str::Parse(curr, "RFC %d", &rfc)) {
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
            Str lineEnd = str::FindChar(curr, '\n');
            if (lineEnd && str::Parse(Str(lineEnd.s + 1, curr.len - (lineEnd.s - curr.s) - 1), "%?\r\n")) {
                htmlData.AppendFmt("<b id='section%d' title=\"", ++sectionCount);
                for (int j = 0; j < (int)(lineEnd.s - curr.s); j++) {
                    char ch = curr.s[j];
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

ByteSlice TxtDoc::GetHtmlData() const {
    return htmlData.AsByteSlice();
}

TempStr TxtDoc::GetPropertyTemp(Str) const {
    return {};
}

Str TxtDoc::GetFileName() const {
    return Str(fileName);
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
    while ((el = parser.FindElementByName(Str("b"), el)) != nullptr) {
        AutoFreeWStr title(el->GetAttribute(Str("title")).s);
        AutoFreeWStr id(el->GetAttribute(Str("id")).s);
        int level = 1;
        if (str::IsDigit(*title)) {
            const WCHAR* dot = SkipDigits(title);
            while ('.' == *dot && str::IsDigit(*(dot + 1))) {
                level++;
                dot = SkipDigits(dot + 1);
            }
        }
        TempStr titleA = ToUtf8Temp(title);
        TempStr idA = ToUtf8Temp(id);
        visitor->Visit(titleA, idA, level);
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
