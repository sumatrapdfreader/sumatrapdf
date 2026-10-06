/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Archive.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/HtmlTags.h"
#if IS_DEBUG
#include "base/Zip.h"
#endif
#if OS_WIN
#include "base/Win.h"
#endif

#include "DocProperties.h"
#include "DocController.h"
#include "EbookBase.h"
#include "GumboHtmlParser.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"
#include "EbookDoc.h"

#if !OS_WIN
static uint GuessTextCodepage(Str, uint defVal) {
    return defVal;
}
#endif

template <typename T>
static T* LoadEbook(Str path) {
    auto* doc = new T(path);
    if (doc && doc->Load()) {
        return doc;
    }
    delete doc;
    return nullptr;
}

static Str TakeArchiveData(Archive* archive, int fileId) {
    auto* fi = archive->GetFileDataById(fileId);
    if (!fi || !fi->data) {
        return {};
    }
    Str res(fi->data, fi->fileSizeUncompressed);
    fi->data = nullptr;
    return res;
}

static TempStr GetXmlPIAttrTemp(Str xmlPI, Str attrName) {
    Str rest(xmlPI.s + 2, len(xmlPI) - 2);
    str::TrimNonWs(rest);
    while (len(rest) > 0) {
        str::TrimWs(rest);
        if (len(rest) == 0 || rest.s[0] == '?' || rest.s[0] == '>') {
            return {};
        }

        int n = 0;
        while (n < len(rest) && !str::IsWs(rest.s[n]) && rest.s[n] != '=' && rest.s[n] != '?' && rest.s[n] != '>') {
            n++;
        }
        Str name(rest.s, n);
        rest = Str(rest.s + n, len(rest) - n);
        str::TrimWs(rest);
        if (!str::TrimPrefix(rest, StrL("="))) {
            continue;
        }
        str::TrimWs(rest);
        if (len(rest) == 0) {
            return {};
        }

        Str val;
        if (rest.s[0] == '"' || rest.s[0] == '\'') {
            char quote = rest.s[0];
            rest = Str(rest.s + 1, len(rest) - 1);
            if (!str::CutChar(rest, quote, &val, &rest)) {
                return {};
            }
        } else {
            val = str::NextWord(rest);
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
    if (!str::StartsWith(xmlPI, StrL("<?xml"))) {
        return CP_ACP;
    }
    int xmlPIEnd = str::IndexOf(xmlPI, StrL("?>"));
    if (xmlPIEnd < 0) {
        return CP_ACP;
    }
    TempStr encoding = GetXmlPIAttrTemp(Str(xmlPI.s, xmlPIEnd + 2), StrL("encoding"));
    if (len(encoding) == 0) {
        return CP_ACP;
    }

    struct {
        Str namePart;
        uint codePage;
    } static encodings[] = {
        {StrL("UTF"), CP_UTF8},
        {StrL("utf"), CP_UTF8},
        {StrL("1252"), 1252},
        {StrL("1251"), 1251},
        // TODO: any other commonly used codepages?
    };
    for (auto& enc : encodings) {
        if (str::Contains(encoding, enc.namePart)) {
            return enc.codePage;
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
        } else if (c < 0xC0) { // NOLINT(bugprone-branch-clone): continuation byte, distinct from the >= 0xF5 case
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
    if (str::TrimPrefix(s, StrL(kUtf8Bom))) {
        return str::DupTemp(s);
    }
    if (str::TrimPrefix(s, StrL(kUtf16Bom))) {
        WStr ws = str::CastStrToWStr(s);
        return ToUtf8Temp(ws);
    }
    if (str::TrimPrefix(s, StrL(kUtf16BeBom))) {
        // convert from utf16 big endian to utf16
        int n = str::CastStrToWStr(s).len;
        for (int i = 0; i < n; i++) {
            int idx = i * 2;
            std::swap(s.s[idx], s.s[idx + 1]);
        }
        WStr ws = str::CastStrToWStr(s);
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
    if (len(url) == 0 || len(base) == 0) {
        // nothing to resolve against; url is already as normalized as it gets
        ReportIf(true);
        return str::DupTemp(url);
    }
    if (url.s[0] == '/' || str::ContainsChar(url, ':')) {
        return str::DupTemp(url);
    }

    str::CutChar(base, '#', &base, nullptr);
    int basePathLen = url.s[0] == '#' ? len(base) : str::LastIndexOfChar(base, '/') + 1;
    TempStr norm = str::JoinTemp(Str(base.s, basePathLen), url);

    // Collapse /./ and /../. For /../, consume only "/.." so the trailing '/'
    // stays for the next iteration — otherwise consecutive ../../ leaves a
    // literal ".." (issue #5846: OEBPS/html/../../cover.jpg → cover.jpg).
    int dst = 0;
    for (int src = 0; src < norm.len; src++) {
        char c = norm.s[src];
        if (c != '/') {
            norm.s[dst++] = c;
        } else if (str::StartsWith(Str(norm.s + src, norm.len - src), StrL("/./"))) {
            src++;
        } else if (str::StartsWith(Str(norm.s + src, norm.len - src), StrL("/../")) ||
                   str::Eq(Str(norm.s + src, norm.len - src), StrL("/.."))) {
            while (dst > 0 && norm.s[dst - 1] != '/') {
                dst--;
            }
            if (dst > 0) {
                dst--; // drop the segment separator; re-added when trailing '/' is processed
            }
            src += 2; // leave trailing '/' (if any) for the next iteration
        } else if (dst > 0) {
            // skip a leading '/' so results stay relative to the ZIP root
            norm.s[dst++] = '/';
        }
    }
    norm.s[dst] = '\0';
    norm.len = dst;
    return norm;
}

static inline char decode64(char c) {
    if ('A' <= c && c <= 'Z') {
        return (char)(c - 'A');
    }
    if ('a' <= c && c <= 'z') {
        return (char)(c - 'a' + 26);
    }
    if ('0' <= c && c <= '9') {
        return (char)(c - '0' + 52);
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
    constexpr int kDigitBits = 6;
    constexpr int kByteBits = 8;
    int sLen = len(data);
    char* s = data.s;
    char* end = data.s + sLen;
    char* result = AllocArrayTemp<char>(sLen * 3 / 4);
    char* curr = result;
    u32 value = 0;
    int bits = 0;
    for (; s < end && *s != '='; s++) {
        char n = decode64(*s);
        if (-1 == n) {
            if (str::IsWs(*s)) {
                continue;
            }
            return {};
        }
        value = (value << kDigitBits) | (u32)n;
        bits += kDigitBits;
        if (bits >= kByteBits) {
            bits -= kByteBits;
            *curr++ = (char)(value >> bits);
        }
    }
    return Str(result, (int)(curr - result));
}

static TempStr DecodeDataURITemp(Str url) {
    Str header, data;
    if (!str::CutChar(url, ',', &header, &data)) {
        return {};
    }
    if (len(header) >= len(StrL("data:;base64")) && str::EndsWith(header, StrL(";base64"))) {
        return Base64DecodeTemp(data);
    }
    return str::DupTemp(data);
}

void FreeImages(Vec<ImageData>& images) {
    for (const ImageData& img : images) {
        str::Free(img.base);
        str::Free(img.fileName);
    }
    VecReset(images);
}

/* ********** EPUB ********** */

EpubDoc::EpubDoc(Str fileName) {
    str::ReplaceWithCopy(&this->fileName, fileName);
    archive = OpenArchiveFromFile(fileName, /*eagerLoad=*/true, gArchiveProgressCb);
}

EpubDoc::~EpubDoc() {
    zipAccess.Lock();

    FreeImages(images);

    zipAccess.Unlock();
    delete archive;
    FreeProps(props);
    str::Free(tocPath);
    str::Free(fileName);
}

static bool isHtmlMediaType(Str mediatype) {
    static SeqStrings types =
        "application/xhtml+xml\0application/html+xml\0application/x-dtbncx+xml\0text/html\0text/xml\0";
    return SeqStrIndex(types, mediatype) >= 0;
}

static bool isImageMediaType(Str mediatype) {
    static SeqStrings types = "image/png\0image/jpeg\0image/gif\0";
    return SeqStrIndex(types, mediatype) >= 0;
}

static void ParseMetadata(Str content, Props& props);

static void CollectEncryptedEpubPaths(const GumboNode* root, StrVec& encList) {
    Vec<const GumboNode*> toVisit;
    VecAppend(toVisit, root);
    while (len(toVisit) > 0) {
        const GumboNode* node = VecPop(toVisit);
        if (!node) {
            continue;
        }
        if (GumboTagNameIs(node, StrL("CipherReference"), HtmlNameMatch::Local)) {
            TempStr uri = GumboAttributeValueTemp(node, "URI");
            if (uri) {
                uri = url::DecodeTemp(uri);
                encList.Append(uri);
            }
        }
        GumboPushChildren(toVisit, node);
    }
}

static Archive::FileInfo* GetEpubPackage(Archive* archive, TempStr& contentPath) {
    auto* containerFi = archive->GetFileDataByName(StrL("META-INF/container.xml"));
    if (!containerFi || !containerFi->data) {
        return nullptr;
    }
    GumboDoc containerDoc(Str(containerFi->data, containerFi->fileSizeUncompressed), GumboMode::XmlFragment);
    // The first rootfile is the default rendition.
    const GumboNode* node = GumboFindDescendantByTag(containerDoc.Document(), StrL("rootfile"), HtmlNameMatch::Local);
    contentPath = url::DecodeTemp(GumboAttributeValueTemp(node, "full-path"));
    if (len(contentPath) == 0) {
        return nullptr;
    }
    auto* fi = archive->GetFileDataByName(contentPath);
    return fi && fi->data ? fi : nullptr;
}

static EpubReadingDirection EpubSpineDirection(const GumboNode* spine) {
    TempStr dir = GumboAttributeValueTemp(spine, "page-progression-direction");
    return {(bool)dir, str::EqI(dir, StrL("rtl"))};
}

bool EpubDoc::Load() {
    if (!archive) {
        return false;
    }
    TempStr contentPath;
    auto* contentFi = GetEpubPackage(archive, contentPath);
    if (!contentFi) {
        return false;
    }

    // encrypted files will be ignored (TODO: support decryption)
    StrVec encList;
    auto* encryptionFi = archive->GetFileDataByName(StrL("META-INF/encryption.xml"));
    if (encryptionFi && encryptionFi->data) {
        Str encryption(encryptionFi->data, encryptionFi->fileSizeUncompressed);
        GumboDoc encryptionDoc(encryption, GumboMode::XmlFragment);
        CollectEncryptedEpubPaths(encryptionDoc.Document(), encList);
    }

    Str content(contentFi->data, contentFi->fileSizeUncompressed);
    ParseMetadata(content, props);
    GumboDoc contentDoc(content, GumboMode::XmlFragment);
    const GumboNode* node = contentDoc.Document();
    if (!node) {
        return false;
    }
    node = GumboFindDescendantByTag(node, StrL("manifest"), HtmlNameMatch::Local);
    if (!node) {
        return false;
    }

    contentPath = Str(contentPath.s, str::LastIndexOfChar(contentPath, '/') + 1);

    StrVec idList, pathList;

    const GumboNode* manifest = node;
    const GumboVector* manifestChildren = GumboChildrenOf(manifest);
    for (unsigned int i = 0; manifestChildren && i < manifestChildren->length; i++) {
        node = (const GumboNode*)manifestChildren->data[i];
        if (!node || node->type != GUMBO_NODE_ELEMENT) {
            continue;
        }
        TempStr mediaType = GumboAttributeValueTemp(node, "media-type");
        bool image = isImageMediaType(mediaType);
        if (!image && !isHtmlMediaType(mediaType)) {
            continue;
        }
        TempStr path = GumboAttributeValueTemp(node, "href");
        if (len(path) == 0) {
            continue;
        }
        path = url::DecodeTemp(path);
        TempStr fullPath = str::JoinTemp(contentPath, path);
        if (image) {
            if (encList.Contains(fullPath)) {
                continue;
            }
            // load the image lazily
            ImageData data;
            data.fileName = str::Dup(fullPath);
            data.fileId = archive->GetFileId(data.fileName);
            VecAppend(images, data);
            continue;
        }
        TempStr htmlId = GumboAttributeValueTemp(node, "id");
        // EPUB 3 ToC
        TempStr properties = GumboAttributeValueTemp(node, "properties");
        if (properties && str::Contains(properties, StrL("nav")) && str::Eq(mediaType, StrL("application/xhtml+xml"))) {
            str::ReplaceWithCopy(&tocPath, fullPath);
        }
        if (encList.Contains(fullPath)) {
            continue;
        }
        if (path && htmlId) {
            idList.Append(htmlId);
            pathList.Append(path);
        }
    }

    node = GumboFindDescendantByTag(contentDoc.Document(), StrL("spine"), HtmlNameMatch::Local);
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
    EpubReadingDirection readingDir = EpubSpineDirection(node);
    if (readingDir.declared) {
        hasReadingDir = true;
        isRtlDoc = readingDir.rtl;
    }

    const GumboNode* spine = node;
    const GumboVector* spineChildren = GumboChildrenOf(spine);
    for (unsigned int i = 0; spineChildren && i < spineChildren->length; i++) {
        node = (const GumboNode*)spineChildren->data[i];
        if (!GumboTagNameIs(node, StrL("itemref"), HtmlNameMatch::Local)) {
            continue;
        }
        TempStr idref = GumboAttributeValueTemp(node, "idref");
        if (len(idref) == 0) {
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
        Str html(htmlFi->data, htmlFi->fileSizeUncompressed);
        TempStr decoded = DecodeTextToUtf8Temp(html, true);
        if (len(decoded) == 0) {
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

// @gen-start docprop-epub
// clang-format off
static SeqStrNum epubPropsMap =
    "dc:title\0" "\x02"
    "dc:creator\0" "\x04"
    "dc:date\0" "\x0a"
    "dcterms:modified\0" "\x0c"
    "dc:description\0" "\x08"
    "dc:rights\0" "\x06"
    "\0";
// clang-format on
// @gen-end docprop-epub

static bool IsTokPropName(HtmlToken* tok, Str name) {
    if (tok->NameIs(name)) {
        return true;
    }
    if (Tag_Meta != tok->tag) {
        return false;
    }
    AttrInfo attr = tok->GetAttrByName(StrL("property"));
    return attr && attr.ValIs(name);
}

static void ParseMetadata(Str content, Props& props) {
    GumboHtmlParser pullParser(content);
    int insideMetadata = 0;
    HtmlToken* tok;

    while ((tok = pullParser.Next()) != nullptr) {
        if (tok->IsStartTag() && tok->NameIs(StrL("metadata"), HtmlNameMatch::Local)) {
            insideMetadata++;
        } else if (tok->IsEndTag() && tok->NameIs(StrL("metadata"), HtmlNameMatch::Local)) {
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
                AddPropOwned(props, (DocProp)propNo, val);
            }
            break;
        }
    }
}

Str EpubDoc::GetImageData(Str fileName, Str pagePath) {
    ScopedMutex scope(&zipAccess);

    bool partial = len(pagePath) == 0;
    ReportIf(partial);
    TempStr url;
    if (!partial) {
        url = NormalizeURLTemp(fileName, pagePath);
        // Some EPUB producers use Windows path separators.
        str::TransCharsInPlace(url, StrL("\\"), StrL("/"));
    }
    for (ImageData& img : images) {
        bool matches = partial ? str::EndsWithI(img.fileName, fileName) : str::Eq(img.fileName, url);
        if (!matches) {
            continue;
        }
        if (len(img.base) == 0) {
            img.base = TakeArchiveData(archive, img.fileId);
        }
        if (len(img.base) > 0) {
            return img.base;
        }
    }
    if (partial) {
        return {};
    }

    // Images need not be registered in the manifest.
    ImageData data;
    data.fileId = archive->GetFileId(url);
    data.base = TakeArchiveData(archive, data.fileId);
    if (!data.base.s) {
        return {};
    }
    data.fileName = str::Dup(url);
    VecAppend(images, data);
    return VecLast(images).base;
}

Str EpubDoc::GetFileData(Str relPath, Str pagePath) {
    if (len(pagePath) == 0) {
        ReportIf(true);
        return {};
    }

    ScopedMutex scope(&zipAccess);
    TempStr url = NormalizeURLTemp(relPath, pagePath);
    return TakeArchiveData(archive, archive->GetFileId(url));
}

TempStr EpubDoc::GetPropertyTemp(DocProp prop) const {
    return GetPropValueTemp(props, prop);
}

static bool ParseNavToc(Str data, Str pagePath, EbookTocVisitor* visitor) {
    GumboHtmlParser parser(data);
    HtmlToken* tok;
    // skip to the start of the <nav epub:type="toc">
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && Tag_Nav == tok->tag) {
            AttrInfo attr = tok->GetAttrByName(StrL("epub:type"));
            if (attr && attr.ValIs(StrL("toc"))) {
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
        if (!tok->IsStartTag() || (Tag_A != tok->tag && Tag_Span != tok->tag)) {
            continue;
        }
        HtmlTag itemTag = tok->tag;
        str::Builder text;
        TempStr href;
        if (Tag_A == tok->tag) {
            AttrInfo attrInfo = tok->GetAttrByName(StrL("href"));
            if (attrInfo) {
                href = str::DupTemp(attrInfo.val);
            }
        }
        while ((tok = parser.Next()) != nullptr && !tok->IsError() && (!tok->IsEndTag() || itemTag != tok->tag)) {
            if (tok->IsText()) {
                text.Append(tok->s);
            }
        }
        if (len(text) == 0) {
            continue;
        }
        TempStr itemText = ToStrTemp(text);
        itemText.len -= str::NormalizeWSInPlace(itemText);
        TempStr itemSrc;
        if (href) {
            TempStr normHref = NormalizeURLTemp(href, pagePath);
            itemSrc = ResolveHtmlEntitiesTemp(normHref);
        }
        visitor->Visit(itemText, itemSrc, level);
    }

    return true;
}

static bool ParseNcxToc(Str data, Str pagePath, EbookTocVisitor* visitor) {
    GumboHtmlParser parser(data);
    HtmlToken* tok;
    // skip to the start of the navMap
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->IsStartTag() && tok->NameIs(StrL("navMap"), HtmlNameMatch::Local)) {
            break;
        }
    }
    if (!tok || tok->IsError()) {
        return false;
    }

    TempStr itemText, itemSrc;
    int level = 0;
    while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
           (!tok->IsEndTag() || !tok->NameIs(StrL("navMap"), HtmlNameMatch::Local))) {
        if (tok->IsTag() && tok->NameIs(StrL("navPoint"), HtmlNameMatch::Local)) {
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
        } else if (tok->IsStartTag() && tok->NameIs(StrL("text"), HtmlNameMatch::Local)) {
            tok = parser.Next();
            if (tok == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                itemText = ResolveHtmlEntitiesTemp(tok->s);
            }
        } else if (tok->IsTag() && !tok->IsEndTag() && tok->NameIs(StrL("content"), HtmlNameMatch::Local)) {
            AttrInfo attrInfo = tok->GetAttrByName(StrL("src"));
            if (attrInfo) {
                TempStr src = NormalizeURLTemp(attrInfo.val, pagePath);
                itemSrc = ResolveHtmlEntitiesTemp(src);
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
    if (len(tocDataStr) == 0) {
        return false;
    }

    Str pagePath = tocPath;
    if (isNcxToc) {
        return ParseNcxToc(tocDataStr, pagePath, visitor);
    }
    return ParseNavToc(tocDataStr, pagePath, visitor);
}

// Only the spine's page-progression-direction. Loading the whole book to read
// one attribute would mean parsing every chapter.
EpubReadingDirection EpubGetReadingDirection(Str path) {
    AutoDelete archive(OpenArchiveFromFile(path, false, gArchiveProgressCb));
    if (!archive) {
        return {};
    }
    TempStr contentPath;
    auto* contentFi = GetEpubPackage(archive, contentPath);
    if (!contentFi) {
        return {};
    }
    GumboDoc doc(Str(contentFi->data, contentFi->fileSizeUncompressed), GumboMode::XmlFragment);
    const GumboNode* spine = GumboFindDescendantByTag(doc.Document(), StrL("spine"), HtmlNameMatch::Local);
    return EpubSpineDirection(spine);
}

EpubDoc* EpubDoc::CreateFromFile(Str path) {
    return LoadEbook<EpubDoc>(path);
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

// Caller-owned cover bytes from EPUB 2 metadata or EPUB 3 cover-image properties.
// Empty when no cover is declared.
Str EpubCoverImageData(Str path) {
    AutoDelete archive(OpenArchiveFromFile(path, false, gArchiveProgressCb));
    if (!archive) {
        return {};
    }
    TempStr contentPath;
    auto* contentFi = GetEpubPackage(archive, contentPath);
    if (!contentFi) {
        return {};
    }
    Str content = Str(contentFi->data, contentFi->fileSizeUncompressed);
    GumboDoc contentDoc(content, GumboMode::XmlFragment);

    TempStr coverId{};
    const GumboNode* node = GumboFindDescendantByTag(contentDoc.Document(), StrL("metadata"), HtmlNameMatch::Local);
    const GumboVector* children = GumboChildrenOf(node);
    for (unsigned int i = 0; children && i < children->length; i++) {
        node = (const GumboNode*)children->data[i];
        if (str::EqI(GumboAttributeValueTemp(node, "name"), StrL("cover"))) {
            coverId = GumboAttributeValueTemp(node, "content");
            break;
        }
    }

    TempStr href{};
    node = GumboFindDescendantByTag(contentDoc.Document(), StrL("manifest"), HtmlNameMatch::Local);
    children = GumboChildrenOf(node);
    for (unsigned int i = 0; children && i < children->length; i++) {
        node = (const GumboNode*)children->data[i];
        if (!isImageMediaType(GumboAttributeValueTemp(node, "media-type"))) {
            continue;
        }
        TempStr properties = GumboAttributeValueTemp(node, "properties");
        bool isCover = len(coverId) > 0 && str::Eq(GumboAttributeValueTemp(node, "id"), coverId);
        if (!isCover && properties) {
            isCover = str::Contains(properties, StrL("cover-image"));
        }
        if (isCover) {
            href = GumboAttributeValueTemp(node, "href");
            break;
        }
    }
    if (len(href) <= 0) {
        return {};
    }
    TempStr imgPath = NormalizeURLTemp(url::DecodeTemp(href), contentPath);
    auto* imgFi = archive->GetFileDataByName(imgPath);
    return imgFi && imgFi->data ? str::Dup(Str(imgFi->data, imgFi->fileSizeUncompressed)) : Str{};
}

/* ********** FictionBook (FB2) ********** */

Fb2Doc::Fb2Doc(Str fileName) : fileName(str::Dup(fileName)) {}

Fb2Doc::~Fb2Doc() {
    str::Free(coverImage);
    FreeImages(images);
    FreeProps(props);
    str::Free(fileName);
}

static Str ReadFb2Archive(Fb2Doc* doc, Archive* archive) {
    AutoDelete delArchive(archive);
    doc->isZipped = true;
    const auto& fileInfos = archive->GetFileInfos();
    if (len(fileInfos) == 0) {
        return {};
    }
    if (len(fileInfos) == 1) {
        return TakeArchiveData(archive, 0);
    }

    // Multi-entry archives contain one FB2 and optional URL shortcuts.
    Str data;
    for (auto* info : fileInfos) {
        if (str::EndsWithI(info->name, StrL(".fb2")) && len(data) == 0) {
            data = TakeArchiveData(archive, info->fileId);
        } else if (!str::EndsWithI(info->name, StrL(".url"))) {
            str::Free(data);
            return {};
        }
    }
    return data;
}

static bool LooksLikeZipOrRar(Str data) {
    if (len(data) < 4) {
        return false;
    }
    return (data.s[0] == 'P' && data.s[1] == 'K') || str::StartsWith(data, StrL("Rar!"));
}

static TempStr JoinEbookTextTemp(Str text, Str part) {
    part = ResolveHtmlEntitiesTemp(part);
    return text ? str::JoinTemp(text, StrL(" "), part) : part;
}

bool Fb2Doc::Load(Str srcData) {
    ReportIf(len(srcData) == 0 && len(fileName) == 0);

    Str data;
    if (len(fileName) > 0) {
        Archive* archive = OpenArchiveFromFile(fileName, /*eagerLoad=*/true, gArchiveProgressCb);
        data = archive ? ReadFb2Archive(this, archive) : file::ReadFile(fileName);
    } else if (srcData) {
        // Plain FB2 XML must bypass libarchive (#1677).
        Archive* archive = LooksLikeZipOrRar(srcData) ? OpenArchiveFromData(srcData) : nullptr;
        data = archive ? ReadFb2Archive(this, archive) : str::Dup(srcData);
    }
    if (len(data) == 0) {
        return false;
    }
    TempStr tmp = DecodeTextToUtf8Temp(data, true);
    str::Free(data);
    if (len(tmp) == 0) {
        return false;
    }

    GumboHtmlParser parser(tmp);
    HtmlToken* tok;
    int inBody = 0, inTitleInfo = 0, inDocInfo = 0;
    Str bodyStart;
    TempStr titleAuthors; // every <author> in <title-info>, joined
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!inTitleInfo && !inDocInfo && tok->IsStartTag() && Tag_Body == tok->tag) {
            if (!inBody++) {
                bodyStart = tok->s;
            }
        } else if (inBody && tok->IsEndTag() && Tag_Body == tok->tag) {
            if (!--inBody) {
                if (len(xmlData) > 0) {
                    xmlData.Append(StrL("<pagebreak />"));
                }
                xmlData.AppendChar('<');
                xmlData.Append(Str(bodyStart.s, (int)(tok->s.s - bodyStart.s) + tok->s.len));
                xmlData.AppendChar('>');
            }
        } else if (inBody && tok->IsStartTag() && Tag_Title == tok->tag) {
            hasToc = true;
        } else if (inBody) { // NOLINT(bugprone-branch-clone): skipping body content is its own case
            continue;
        } else if (inTitleInfo && tok->IsEndTag() && tok->NameIs(StrL("title-info"), HtmlNameMatch::Local)) {
            inTitleInfo--;
        } else if (inDocInfo && tok->IsEndTag() && tok->NameIs(StrL("document-info"), HtmlNameMatch::Local)) {
            inDocInfo--;
        } else if (tok->IsStartTag() && ((inTitleInfo && tok->NameIs(StrL("book-title"), HtmlNameMatch::Local)) ||
                                         (inDocInfo && tok->NameIs(StrL("program-used"), HtmlNameMatch::Local)))) {
            DocProp prop = tok->NameIs(StrL("book-title"), HtmlNameMatch::Local) ? DocProp::Title : DocProp::CreatorApp;
            tok = parser.Next();
            if (tok == nullptr || tok->IsError()) {
                break;
            }
            if (tok->IsText()) {
                TempStr val = ResolveHtmlEntitiesTemp(tok->s);
                AddPropOwned(props, prop, val);
            }
        } else if ((inTitleInfo || inDocInfo) && tok->IsStartTag() &&
                   tok->NameIs(StrL("author"), HtmlNameMatch::Local)) {
            // an FB2 <author> is structured: first-name / middle-name / last-name
            // next to home-page / email / id, which are not part of the name.
            // Taking every text node would give "Ivan Petrov https://... ivan@..."
            // (issue #2254)
            TempStr docAuthor;
            TempStr nickname;
            bool inNamePart = false;
            bool inNickname = false;
            while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
                   !(tok->IsEndTag() && tok->NameIs(StrL("author"), HtmlNameMatch::Local))) {
                if (tok->IsStartTag() || tok->IsEndTag()) {
                    bool isName = tok->NameIs(StrL("first-name"), HtmlNameMatch::Local) ||
                                  tok->NameIs(StrL("middle-name"), HtmlNameMatch::Local) ||
                                  tok->NameIs(StrL("last-name"), HtmlNameMatch::Local);
                    if (isName) {
                        inNamePart = tok->IsStartTag();
                    } else if (tok->NameIs(StrL("nickname"), HtmlNameMatch::Local)) {
                        inNickname = tok->IsStartTag();
                    }
                    continue;
                }
                if (!tok->IsText()) {
                    continue;
                }
                if (inNamePart) {
                    docAuthor = JoinEbookTextTemp(docAuthor, tok->s);
                } else if (inNickname) {
                    nickname = JoinEbookTextTemp(nickname, tok->s);
                }
            }
            if (len(docAuthor) == 0) {
                // some files give only a nickname
                docAuthor = nickname;
            }
            if (docAuthor) {
                docAuthor.len -= str::NormalizeWSInPlace(docAuthor);
                if (len(docAuthor) > 0) {
                    if (inTitleInfo) {
                        // a book can list several authors; report all of them
                        titleAuthors = titleAuthors ? str::JoinTemp(titleAuthors, StrL(", "), docAuthor) : docAuthor;
                        AddPropOwned(props, DocProp::Author, titleAuthors, true);
                    } else {
                        AddPropOwned(props, DocProp::Author, docAuthor, false);
                    }
                }
            }
        } else if ((inTitleInfo || inDocInfo) && tok->IsStartTag() && tok->NameIs(StrL("date"), HtmlNameMatch::Local)) {
            AttrInfo attr = tok->GetAttrByName(StrL("value"), HtmlNameMatch::Local);
            if (attr) {
                TempStr val = ResolveHtmlEntitiesTemp(attr.val);
                AddPropOwned(props, inTitleInfo ? DocProp::CreationDate : DocProp::ModificationDate, val);
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIs(StrL("coverpage"), HtmlNameMatch::Local)) {
            tok = parser.Next();
            if (tok && tok->IsText()) {
                tok = parser.Next();
            }
            if (tok && tok->IsEmptyElementEndTag() && Tag_Image == tok->tag) {
                AttrInfo attr = tok->GetAttrByName(StrL("href"), HtmlNameMatch::Local);
                if (attr) {
                    str::ReplaceWithCopy(&coverImage, attr.val);
                }
            }
        } else if (inTitleInfo && tok->IsStartTag() && tok->NameIs(StrL("annotation"), HtmlNameMatch::Local)) {
            // FB2 annotation is nested markup (often one or more <p>); collect all text for
            // Document Properties (Ctrl+D) as Subject.
            TempStr annotation;
            while ((tok = parser.Next()) != nullptr && !tok->IsError() &&
                   !(tok->IsEndTag() && tok->NameIs(StrL("annotation"), HtmlNameMatch::Local))) {
                if (tok->IsText()) {
                    annotation = JoinEbookTextTemp(annotation, tok->s);
                }
            }
            if (annotation) {
                annotation.len -= str::NormalizeWSInPlace(annotation);
                if (len(annotation) > 0) {
                    AddPropOwned(props, DocProp::Subject, annotation);
                }
            }
        } else if (inTitleInfo || inDocInfo) {
            continue;
        } else if (tok->IsStartTag() && tok->NameIs(StrL("title-info"), HtmlNameMatch::Local)) {
            inTitleInfo++;
        } else if (tok->IsStartTag() && tok->NameIs(StrL("document-info"), HtmlNameMatch::Local)) {
            inDocInfo++;
        } else if (tok->IsStartTag() && tok->NameIs(StrL("binary"), HtmlNameMatch::Local)) {
            ExtractImage(&parser, tok);
        }
    }

    return len(xmlData) > 0;
}

void Fb2Doc::ExtractImage(GumboHtmlParser* parser, HtmlToken* tok) {
    TempStr id;
    AttrInfo attrInfo = tok->GetAttrByName(StrL("id"), HtmlNameMatch::Local);
    if (attrInfo) {
        id = url::DecodeTemp(attrInfo.val);
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
    VecAppend(images, data);
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

bool Fb2Doc::ParseToc(EbookTocVisitor* visitor) const {
    TempStr itemText;
    bool inTitle = false;
    int titleCount = 0;
    int level = 0;

    auto xmlData2 = ToStr(xmlData);
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
                TempStr url = fmt(kFb2TocEntryMark "%d", titleCount);
                visitor->Visit(itemText, url, level);
                itemText = {};
            }
            inTitle = false;
        } else if (inTitle && tok->IsText()) {
            itemText = JoinEbookTextTemp(itemText, tok->s);
        }
    }

    return true;
}

Fb2Doc* Fb2Doc::CreateFromFile(Str path) {
    return LoadEbook<Fb2Doc>(path);
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

#define kPdbTocEntryMark "ToC!Entry!"

// http://wiki.mobileread.com/wiki/TealDoc
static Str HandleTealDocTag(str::Builder& builder, StrVec& tocEntries, Str text) {
    if (len(text) < 9) {
    Fallback:
        builder.Append(StrL("&lt;"));
        return text;
    }
    if (!str::StartsWithI(text, StrL("<BOOKMARK")) && !str::StartsWithI(text, StrL("<HEADER")) &&
        !str::StartsWithI(text, StrL("<HRULE")) && !str::StartsWithI(text, StrL("<LABEL")) &&
        !str::StartsWithI(text, StrL("<LINK")) && !str::StartsWithI(text, StrL("<TEALPAINT"))) {
        goto Fallback;
    }
    GumboHtmlParser parser(text);
    HtmlToken* tok = parser.Next();
    if (!tok || !tok->IsStartTag()) {
        goto Fallback;
    }

    if (tok->NameIs(StrL("BOOKMARK"))) {
        // <BOOKMARK NAME="Contents">
        AttrInfo attr = tok->GetAttrByName(StrL("NAME"));
        if (!attr || len(attr.val) == 0) {
            goto Fallback;
        }
        tocEntries.Append(ResolveHtmlEntitiesTemp(attr.val));
        builder.Append(fmt("<a name=" kPdbTocEntryMark "%d>", ::len(tocEntries)));
    } else if (tok->NameIs(StrL("HEADER"))) {
        // <HEADER TEXT="Contents" ALIGN=CENTER STYLE=UNDERLINE>
        int hx = 2;
        AttrInfo attr = tok->GetAttrByName(StrL("FONT"));
        if (attr && attr.val) {
            char font = attr.val.s[0];
            hx = 3;
            if (font == '0') {
                hx = 5;
            } else if (font == '2') {
                hx = 1;
            }
        }
        attr = tok->GetAttrByName(StrL("TEXT"));
        if (!attr) {
            goto Fallback;
        }
        builder.Append(fmt("<h%d>%s</h%d>", hx, attr.val, hx));
    } else if (tok->NameIs(StrL("HRULE"))) {
        // <HRULE STYLE=OUTLINE>
        builder.Append(StrL("<hr>"));
    } else if (tok->NameIs(StrL("LABEL"))) {
        // <LABEL NAME="Contents">
        AttrInfo attr = tok->GetAttrByName(StrL("NAME"));
        if (!attr || len(attr.val) == 0) {
            goto Fallback;
        }
        builder.Append(fmt("<a name=\"%s\">", attr.val));
    } else if (tok->NameIs(StrL("LINK"))) {
        // <LINK TEXT="Press Me" TAG="Contents" FILE="My Novels">
        AttrInfo attrTag = tok->GetAttrByName(StrL("TAG"));
        AttrInfo attrText = tok->GetAttrByName(StrL("TEXT"));
        if (!attrTag || !attrText) {
            goto Fallback;
        }
        // Skip links to other files.
        if (!tok->GetAttrByName(StrL("FILE"))) {
            builder.Append(fmt("<a href=\"#%s\">%s</a>", attrTag.val, attrText.val));
        }
    } else if (!tok->NameIs(StrL("TEALPAINT"))) {
        goto Fallback;
    }
    return Str(tok->s.s + len(tok->s), (int)(text.s + len(text) - (tok->s.s + len(tok->s))));
}

bool PalmDoc::Load() {
    AutoDelete mobiDoc(MobiDoc::CreateFromFile(fileName));
    if (!mobiDoc) {
        return false;
    }
    auto docType = mobiDoc->docType;
    if (docType != PdbDocType::PalmDoc && docType != PdbDocType::TealDoc && docType != PdbDocType::Plucker) {
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
            htmlData.Append(StrL("&amp;"));
        } else if ('<' == c) {
            Str after = HandleTealDocTag(htmlData, tocEntries, Str(rest.s + i, rest.len - i));
            if (after) {
                i += (int)(after.s - (rest.s + i)) - 1;
            }
        } else if ('\n' == c || ('\r' == c && i + 1 < rest.len && '\n' != rest.s[i + 1])) {
            htmlData.Append(StrL("\n<br>"));
        } else {
            htmlData.AppendChar(c);
        }
    }

    return true;
}

bool PalmDoc::ParseToc(EbookTocVisitor* visitor) {
    for (int i = 0; i < len(tocEntries); i++) {
        TempStr url = fmt(kPdbTocEntryMark "%d", i + 1);
        Str name = tocEntries[i];
        visitor->Visit(name, url, 1);
    }
    return true;
}

PalmDoc* PalmDoc::CreateFromFile(Str path) {
    return LoadEbook<PalmDoc>(path);
}

/* ********** Plain HTML ********** */

HtmlDoc::HtmlDoc(Str path) : fileName(str::Dup(path)) {}

HtmlDoc::~HtmlDoc() {
    FreeImages(images);
    FreeProps(props);
    str::Free(htmlData);
    str::Free(fileName);
    str::Free(pagePath);
}

bool HtmlDoc::Load() {
    {
        Str data = file::ReadFile(fileName);
        if (len(data) == 0) {
            return false;
        }
        TempStr decoded = DecodeTextToUtf8Temp(data, true);
        if (len(decoded) == 0) {
            return false;
        }
        htmlData = str::Dup(decoded);
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
                AddPropOwned(props, DocProp::Title, val);
            }
        } else if ((tok->IsStartTag() || tok->IsEmptyElementEndTag()) && Tag_Meta == tok->tag) {
            AttrInfo attrName = tok->GetAttrByName(StrL("name"));
            AttrInfo attrValue = tok->GetAttrByName(StrL("content"));
            if (!attrName || !attrValue) {
                continue;
            }
            DocProp prop = attrName.ValIs(StrL("author"))      ? DocProp::Author
                           : attrName.ValIs(StrL("date"))      ? DocProp::CreationDate
                           : attrName.ValIs(StrL("copyright")) ? DocProp::Copyright
                                                               : DocProp::None;
            if (prop != DocProp::None) {
                AddPropOwned(props, prop, ResolveHtmlEntitiesTemp(attrValue.val));
            }
        }
    }

    return true;
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
    VecAppend(images, data);
    return VecLast(images).base;
}

Str HtmlDoc::GetFileData(Str relPath) {
    TempStr url = NormalizeURLTemp(relPath, pagePath);
    return LoadURL(url);
}

Str HtmlDoc::LoadURL(Str url) {
    AutoArenaSavepoint tempScope;
    if (str::StartsWith(url, StrL("data:"))) {
        return str::Dup(DecodeDataURITemp(url));
    }
    if (str::ContainsChar(url, ':')) {
        return {};
    }
    TempStr path = str::DupTemp(url);
    str::TransCharsInPlace(path, StrL("/"), StrL("\\"));
    return file::ReadFile(path);
}

TempStr HtmlDoc::GetPropertyTemp(DocProp prop) const {
    return GetPropValueTemp(props, prop);
}

HtmlDoc* HtmlDoc::CreateFromFile(Str path) {
    return LoadEbook<HtmlDoc>(path);
}

#if IS_DEBUG
bool EbookDoc_UnitTestLoading() {
    const Str encoded[][2] = {
        {StrL("Zg=="), StrL("f")},          {StrL("Zm8="), StrL("fo")},      {StrL("Z m9v\nYmFy"), StrL("foobar")},
        {StrL("/wD+"), StrL("\xff\0\xfe")}, {StrL("Zg=ignored"), StrL("f")}, {StrL("Zm!8="), {}},
    };
    for (const auto& c : encoded) {
        if (!str::Eq(Base64DecodeTemp(c[0]), c[1])) {
            return false;
        }
    }

    const Str declarations[][2] = {
        {StrL("<?xml encoding=\"UTF-8\"?>"), StrL("UTF-8")},
        {StrL("<?xml version='1.0' ENCODING = 'windows-1252' ?>"), StrL("windows-1252")},
        {StrL("<?xml\tencoding=1251 ?>"), StrL("1251")},
        {StrL("<?xml ignored encoding='UTF-8'?>"), StrL("UTF-8")},
        {StrL("<?xml encoding=''?>"), {}},
        {StrL("<?xml encoding='UTF-8?>"), {}},
        {StrL("<?xml encoding= "), {}},
        {StrL("<?xml?>"), {}},
    };
    for (const auto& c : declarations) {
        if (!str::Eq(GetXmlPIAttrTemp(c[0], StrL("encoding")), c[1])) {
            return false;
        }
    }

    Str xml = StrL("<FictionBook><body><section><p>Shared loading</p></section></body></FictionBook>");
    TempStr path = GetTempFilePathTemp(StrL("ebook-loading-"));
    AutoCall removeFile(file::Delete, path);
    AutoDelete plain(Fb2Doc::CreateFromData(xml));
    if (!plain || !file::WriteFile(path, xml)) {
        return false;
    }
    AutoDelete plainFile(Fb2Doc::CreateFromFile(path));
    if (!plainFile) {
        return false;
    }

    const struct {
        SeqStrings names;
        bool valid;
    } cases[] = {
        {"book.fb2\0", true},
        {"book.txt\0", true},
        {"book.fb2\0info.url\0", true},
        {"info.url\0book.fb2\0", true},
        {"book.fb2\0other.fb2\0", false},
        {"book.fb2\0other.txt\0", false},
        {"other.txt\0info.url\0", false},
    };
    for (const auto& c : cases) {
        str::Builder zip;
        ZipCreator creator(zip);
        for (Str name = SeqStrFirst(c.names); name; name = SeqStrNext(name)) {
            if (!creator.AddFileData(name, xml)) {
                return false;
            }
        }
        if (!creator.Finish() || !file::WriteFile(path, ToStr(zip))) {
            return false;
        }
        AutoDelete dataDoc(Fb2Doc::CreateFromData(ToStr(zip)));
        AutoDelete fileDoc(Fb2Doc::CreateFromFile(path));
        if ((dataDoc.o != nullptr) != c.valid || (fileDoc.o != nullptr) != c.valid) {
            return false;
        }
    }

    // Direction lookup needs only the package, even without chapter contents.
    const struct {
        Str attr;
        EpubReadingDirection expected;
    } directions[] = {
        {StrL("page-progression-direction='rtl'"), {true, true}},
        {StrL("page-progression-direction='RTL'"), {true, true}},
        {StrL("page-progression-direction='ltr'"), {true, false}},
        {StrL("page-progression-direction='default'"), {true, false}},
        {StrL("page-progression-direction=''"), {false, false}},
        {{}, {false, false}},
    };
    for (const auto& c : directions) {
        str::Builder zip;
        ZipCreator creator(zip);
        Str container = StrL("<container><rootfiles><rootfile full-path='OEBPS/package.opf'/></rootfiles></container>");
        if (!creator.AddFileData(StrL("META-INF/container.xml"), container) ||
            !creator.AddFileData(StrL("OEBPS/package.opf"), fmt("<package><spine %s/></package>", c.attr)) ||
            !creator.Finish() || !file::WriteFile(path, ToStr(zip))) {
            return false;
        }
        EpubReadingDirection dir = EpubGetReadingDirection(path);
        if (dir.declared != c.expected.declared || dir.rtl != c.expected.rtl) {
            return false;
        }
    }
    return true;
}

// issue #5846: consecutive ../../ must fully resolve
bool EbookDoc_UnitTestNormalizeURL() {
    const Str cases[][3] = {
        {StrL("../../cover.jpg"), StrL("OEBPS/html/titlepage.xhtml"), StrL("cover.jpg")},
        {StrL("../../root.jpg"), StrL("OEBPS/html/page.xhtml"), StrL("root.jpg")},
        {StrL("../../img/c.jpg"), StrL("a/b/p.xhtml"), StrL("img/c.jpg")},
        {StrL("../../../c.jpg"), StrL("a/b/x/p.xhtml"), StrL("c.jpg")},
        {StrL("../ok.jpg"), StrL("OEBPS/html/page.xhtml"), StrL("OEBPS/ok.jpg")},
        {StrL("../Images/x.jpg"), StrL("OEBPS/Text/y.xhtml"), StrL("OEBPS/Images/x.jpg")},
        {StrL("text/../cover.jpg"), StrL("page.xhtml"), StrL("cover.jpg")},
        {StrL("./y"), StrL("x/z"), StrL("x/y")},
        {StrL("../../b"), StrL("a/c.xhtml"), StrL("b")},
        {StrL("/abs/path"), StrL("OEBPS/html/p.xhtml"), StrL("/abs/path")},
        {StrL("http://example.com/x"), StrL("OEBPS/html/p.xhtml"), StrL("http://example.com/x")},
        {StrL("#frag"), StrL("OEBPS/html/p.xhtml#old"), StrL("OEBPS/html/p.xhtml#frag")},
        {StrL("cover.jpg"), StrL("OEBPS/p.xhtml#old/path"), StrL("OEBPS/cover.jpg")},
        {StrL("cover.jpg"), StrL("p.xhtml#old/path"), StrL("cover.jpg")},
        {StrL("#new"), StrL("OEBPS/p.xhtml#old/path"), StrL("OEBPS/p.xhtml#new")},
        {StrL("cover.jpg"), StrL("#old/path"), StrL("cover.jpg")},
    };
    for (const auto& c : cases) {
        if (!str::Eq(NormalizeURLTemp(c[0], c[1]), c[2])) {
            return false;
        }
    }
    return true;
}
#endif
