/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookDoc.h"

#include "ByteReader.h"
#include "FileUtil.h"
#include "HtmlPullParser.h"
#include "MobiDoc.h"
#include "PdbReader.h"
#include "TrivialHtmlParser.h"
#include "WinUtil.h"
#include "ZipUtil.h"

inline WCHAR *FromHtmlUtf8(const char *s, size_t len)
{
    ScopedMem<char> tmp(str::DupN(s, len));
    return DecodeHtmlEntitites(tmp, CP_UTF8);
}

static char *DecodeTextToUtf8(const char *s)
{
    ScopedMem<char> tmp;
    int one = 1;
    bool isBE = !*(char *)&one;
    if (str::StartsWith(s, isBE ? UTF16_BOM : UTF16BE_BOM)) {
        // swap endianness to match the local architecture's
        size_t byteCount = (str::Len((WCHAR *)s) + 1) * sizeof(WCHAR);
        tmp.Set((char *)memdup(s, byteCount));
        for (size_t i = 0; i < byteCount; i += 2) {
            swap(tmp[i], tmp[i+1]);
        }
        s = tmp;
    }
    if (str::StartsWith(s, isBE ? UTF16BE_BOM : UTF16_BOM))
        return str::ToMultiByte((WCHAR *)s, CP_UTF8);
    if (str::StartsWith(s, UTF8_BOM))
        return str::Dup(s);
    UINT codePage = GuessTextCodepage(s, str::Len(s), CP_ACP);
    return str::ToMultiByte(s, codePage, CP_UTF8);
}

// tries to extract an encoding from <?xml encoding="..."?>
// returns CP_ACP on failure
static UINT GetCodepageFromPI(const char *xmlPI)
{
    const char *xmlPIEnd = str::Find(xmlPI, "?>");
    if (!xmlPIEnd)
        return CP_ACP;
    HtmlToken pi;
    pi.SetTag(HtmlToken::EmptyElementTag, xmlPI + 2, xmlPIEnd);
    pi.nLen = 4;
    AttrInfo *enc = pi.GetAttrByName("encoding");
    if (!enc)
        return CP_ACP;

    ScopedMem<char> encoding(str::DupN(enc->val, enc->valLen));
    struct {
        char *namePart;
        UINT codePage;
    } encodings[] = {
        { "UTF", CP_UTF8 }, { "utf", CP_UTF8 },
        { "1252", 1252 }, { "1251", 1251 },
        // TODO: any other commonly used codepages?
    };
    for (size_t i = 0; i < dimof(encodings); i++) {
        if (str::Find(encoding, encodings[i].namePart))
            return encodings[i].codePage;
    }
    return CP_ACP;
}

char *NormalizeURL(const char *url, const char *base)
{
    CrashIf(!url || !base);
    if (*url == '/' || str::FindChar(url, ':'))
        return str::Dup(url);

    const char *baseEnd = str::FindCharLast(base, '/');
    const char *hash = str::FindChar(base, '#');
    if (baseEnd && hash && hash < baseEnd) {
        for (baseEnd = hash - 1; baseEnd > base && *baseEnd != '/'; baseEnd--);
    }
    if (baseEnd)
        baseEnd++;
    else
        baseEnd = base;
    ScopedMem<char> basePath(str::DupN(base, baseEnd - base));
    ScopedMem<char> norm(str::Join(basePath, url));

    char *dst = norm;
    for (char *src = norm; *src; src++) {
        if (*src != '/')
            *dst++ = *src;
        else if (str::StartsWith(src, "/./"))
            src++;
        else if (str::StartsWith(src, "/../")) {
            for (; dst > norm && *(dst - 1) != '/'; dst--);
            src += 3;
        }
        else
            *dst++ = '/';
    }
    *dst = '\0';
    return norm.StealData();
}

inline char decode64(char c)
{
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

static char *Base64Decode(const char *s, const char *end, size_t *len)
{
    size_t bound = (end - s) * 3 / 4;
    char *result = AllocArray<char>(bound);
    char *curr = result;
    unsigned char c = 0;
    int step = 0;
    for (; s < end && *s != '='; s++) {
        char n = decode64(*s);
        if (-1 == n) {
            if (str::IsWs(*s))
                continue;
            free(result);
            return NULL;
        }
        switch (step++ % 4) {
        case 0: c = n; break;
        case 1: *curr++ = (c << 2) | (n >> 4); c = n & 0xF; break;
        case 2: *curr++ = (c << 4) | (n >> 2); c = n & 0x3; break;
        case 3: *curr++ = (c << 6) | (n >> 0); break;
        }
    }
    if (len)
        *len = curr - result;
    return result;
}

static inline void AppendChar(str::Str<char>& htmlData, char c)
{
    switch (c) {
    case '&': htmlData.Append("&amp;"); break;
    case '<': htmlData.Append("&lt;"); break;
    case '"': htmlData.Append("&quot;"); break;
    default:  htmlData.Append(c); break;
    }
}

/* ********** EPUB ********** */

const char *EPUB_CONTAINER_NS = "urn:oasis:names:tc:opendocument:xmlns:container";
const char *EPUB_OPF_NS = "http://www.idpf.org/2007/opf";
const char *EPUB_NCX_NS = "http://www.daisy.org/z3986/2005/ncx/";

EpubDoc::EpubDoc(const WCHAR *fileName) :
    zip(fileName), fileName(str::Dup(fileName)), isNcxToc(false), isRtlDoc(false) { }

EpubDoc::EpubDoc(IStream *stream) :
    zip(stream), fileName(NULL), isNcxToc(false), isRtlDoc(false) { }

EpubDoc::~EpubDoc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).base.data);
        free(images.At(i).id);
    }
    for (size_t i = 0; i < props.Count(); i++) {
        free(props.At(i).value);
    }
}

bool EpubDoc::Load()
{
    ScopedMem<char> container(zip.GetFileData(L"META-INF/container.xml"));
    if (!container)
        return false;
    HtmlParser parser;
    HtmlElement *node = parser.ParseInPlace(container);
    if (!node)
        return false;
    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByNameNS("rootfile", EPUB_CONTAINER_NS);
    if (!node)
        return false;
    ScopedMem<WCHAR> contentPath(node->GetAttribute("full-path"));
    if (!contentPath)
        return false;

    ScopedMem<WCHAR> contentPathDec(str::Dup(contentPath));
    str::UrlDecodeInPlace(contentPathDec);
    ScopedMem<char> content(zip.GetFileData(contentPathDec));
    if (!content)
        return false;
    ParseMetadata(content);
    node = parser.ParseInPlace(content);
    if (!node)
        return false;
    node = parser.FindElementByNameNS("manifest", EPUB_OPF_NS);
    if (!node)
        return false;

    if (str::FindChar(contentPath, '/'))
        *(WCHAR *)(str::FindCharLast(contentPath, '/') + 1) = '\0';
    else
        *contentPath = '\0';

    WStrList idList, pathList;

    for (node = node->down; node; node = node->next) {
        ScopedMem<WCHAR> mediatype(node->GetAttribute("media-type"));
        if (str::Eq(mediatype, L"image/png")  ||
            str::Eq(mediatype, L"image/jpeg") ||
            str::Eq(mediatype, L"image/gif")) {
            ScopedMem<WCHAR> imgPath(node->GetAttribute("href"));
            if (!imgPath)
                continue;
            imgPath.Set(str::Join(contentPath, imgPath));
            // load the image lazily
            ImageData2 data = { 0 };
            data.id = str::conv::ToUtf8(imgPath);
            str::UrlDecodeInPlace(imgPath);
            data.idx = zip.GetFileIndex(imgPath);
            images.Append(data);
        }
        else if (str::Eq(mediatype, L"application/xhtml+xml") ||
                 str::Eq(mediatype, L"application/x-dtbncx+xml") ||
                 str::Eq(mediatype, L"text/html") ||
                 str::Eq(mediatype, L"text/xml")) {
            ScopedMem<WCHAR> htmlPath(node->GetAttribute("href"));
            ScopedMem<WCHAR> htmlId(node->GetAttribute("id"));
            // EPUB 3 ToC
            ScopedMem<WCHAR> props(node->GetAttribute("properties"));
            if (props && str::Find(props, L"nav") && str::Eq(mediatype, L"application/xhtml+xml")) {
                tocPath.Set(str::Join(contentPath, htmlPath));
                str::UrlDecodeInPlace(tocPath);
            }
            if (htmlPath && htmlId) {
                idList.Append(htmlId.StealData());
                pathList.Append(htmlPath.StealData());
            }
        }
    }

    node = parser.FindElementByNameNS("spine", EPUB_OPF_NS);
    if (!node)
        return false;
    // EPUB 2 ToC
    ScopedMem<WCHAR> tocId(node->GetAttribute("toc"));
    if (tocId && !tocPath && idList.Find(tocId) != -1) {
        tocPath.Set(str::Join(contentPath, pathList.At(idList.Find(tocId))));
        str::UrlDecodeInPlace(tocPath);
        isNcxToc = true;
    }
    ScopedMem<WCHAR> readingDir(node->GetAttribute("page-progression-direction"));
    if (readingDir)
        isRtlDoc = str::EqI(readingDir, L"rtl");

    for (node = node->down; node; node = node->next) {
        if (!node->NameIsNS("itemref", EPUB_OPF_NS))
            continue;
        ScopedMem<WCHAR> idref(node->GetAttribute("idref"));
        if (!idref || idList.Find(idref) == -1)
            continue;

        ScopedMem<WCHAR> fullPath(str::Join(contentPath, pathList.At(idList.Find(idref))));
        ScopedMem<char> utf8_path(str::conv::ToUtf8(fullPath));
        str::UrlDecodeInPlace(fullPath);
        ScopedMem<char> html(zip.GetFileData(fullPath));
        if (!html)
            continue;
        // insert explicit page-breaks between sections including
        // an anchor with the file name at the top (for internal links)
        htmlData.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", utf8_path);
        htmlData.Append(html);
    }

    return htmlData.Count() > 0;
}

void EpubDoc::ParseMetadata(const char *content)
{
    Metadata metadataMap[] = {
        { Prop_Title,           "dc:title" },
        { Prop_Author,          "dc:creator" },
        { Prop_CreationDate,    "dc:date" },
        { Prop_ModificationDate,"dcterms:modified" },
        { Prop_Subject,         "dc:description" },
        { Prop_Copyright,       "dc:rights" },
    };

    HtmlPullParser pullParser(content, str::Len(content));
    int insideMetadata = 0;
    HtmlToken *tok;

    while ((tok = pullParser.Next())) {
        if (tok->IsStartTag() && tok->NameIsNS("metadata", EPUB_OPF_NS))
            insideMetadata++;
        else if (tok->IsEndTag() && tok->NameIsNS("metadata", EPUB_OPF_NS))
            insideMetadata--;
        if (!insideMetadata)
            continue;
        if (!tok->IsStartTag())
            continue;

        for (int i = 0; i < dimof(metadataMap); i++) {
            if (tok->NameIs(metadataMap[i].value) ||
                Tag_Meta == tok->tag && tok->GetAttrByName("property") &&
                tok->GetAttrByName("property")->ValIs(metadataMap[i].value)) {
                tok = pullParser.Next();
                if (!tok || !tok->IsText())
                    break;
                Metadata prop = { metadataMap[i].prop, NULL };
                prop.value = ResolveHtmlEntities(tok->s, tok->sLen);
                if (prop.value)
                    props.Append(prop);
                break;
            }
        }
    }
}

const char *EpubDoc::GetTextData(size_t *lenOut)
{
    *lenOut = htmlData.Size();
    return htmlData.Get();
}

size_t EpubDoc::GetTextDataSize()
{
    return htmlData.Size();
}

ImageData *EpubDoc::GetImageData(const char *id, const char *pagePath)
{
    if (!pagePath) {
        // if we're reparsing, we might not have pagePath, which is needed to
        // build the exact url so try to find a partial match
        // TODO: the correct approach would be to extend reparseIdx into a
        // struct ReparseData, which would include pagePath and all other
        // styling related state (such as nextPageStyle, listDepth, etc. including
        // format specific state such as hiddenDepth and titleCount) and store it
        // in every HtmlPage, but this should work well enough for now
        for (size_t i = 0; i < images.Count(); i++) {
            ImageData2 *img = &images.At(i);
            if (str::EndsWithI(img->id, id)) {
                if (!img->base.data)
                    img->base.data = zip.GetFileData(img->idx, &img->base.len);
                if (img->base.data)
                    return &img->base;
            }
        }
        return NULL;
    }

    ScopedMem<char> url(NormalizeURL(id, pagePath));
    for (size_t i = 0; i < images.Count(); i++) {
        ImageData2 *img = &images.At(i);
        if (str::Eq(img->id, url)) {
            if (!img->base.data)
                img->base.data = zip.GetFileData(img->idx, &img->base.len);
            if (img->base.data)
                return &img->base;
        }
    }
    return NULL;
}

char *EpubDoc::GetFileData(const char *relPath, const char *pagePath, size_t *lenOut)
{
    if (!pagePath)
        return NULL;

    ScopedMem<char> url(NormalizeURL(relPath, pagePath));
    ScopedMem<WCHAR> zipPath(str::conv::FromUtf8(url));
    return zip.GetFileData(zipPath, lenOut);
}

WCHAR *EpubDoc::GetProperty(DocumentProperty prop) const
{
    for (size_t i = 0; i < props.Count(); i++) {
        if (props.At(i).prop == prop)
            return str::conv::FromUtf8(props.At(i).value);
    }
    return NULL;
}

const WCHAR *EpubDoc::GetFileName() const
{
    return fileName;
}

bool EpubDoc::IsRTL() const
{
    return isRtlDoc;
}

bool EpubDoc::HasToc() const
{
    return tocPath != NULL;
}

bool EpubDoc::ParseNavToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor)
{
    HtmlPullParser parser(data, dataLen);
    HtmlToken *tok;
    // skip to the start of the <nav epub:type="toc">
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (tok->IsStartTag() && Tag_Nav == tok->tag) {
            AttrInfo *attr = tok->GetAttrByName("epub:type");
            if (attr && attr->ValIs("toc"))
                break;
        }
    }
    if (!tok || tok->IsError())
        return false;

    int level = 0;
    while ((tok = parser.Next()) && !tok->IsError() && (!tok->IsEndTag() || Tag_Nav != tok->tag)) {
        if (tok->IsStartTag() && Tag_Ol == tok->tag)
            level++;
        else if (tok->IsEndTag() && Tag_Ol == tok->tag && level > 0)
            level--;
        if (tok->IsStartTag() && Tag_A == tok->tag) {
            AttrInfo *attrInfo = tok->GetAttrByName("href");
            if (!attrInfo)
                continue;
            ScopedMem<char> href(str::DupN(attrInfo->val, attrInfo->valLen));
            ScopedMem<char> text;
            while ((tok = parser.Next()) && !tok->IsError() && (!tok->IsEndTag() || Tag_A != tok->tag)) {
                if (tok->IsText()) {
                    ScopedMem<char> part(str::DupN(tok->s, tok->sLen));
                    if (!text)
                        text.Set(part.StealData());
                    else
                        text.Set(str::Join(text, part));
                }
            }
            href.Set(NormalizeURL(href, pagePath));
            ScopedMem<WCHAR> itemSrc(FromHtmlUtf8(href, str::Len(href)));
            ScopedMem<WCHAR> itemText(str::conv::FromUtf8(text));
            str::NormalizeWS(itemText);
            visitor->Visit(itemText, itemSrc, level);
        }
    }

    return true;
}

bool EpubDoc::ParseNcxToc(const char *data, size_t dataLen, const char *pagePath, EbookTocVisitor *visitor)
{
    HtmlPullParser parser(data, dataLen);
    HtmlToken *tok;
    // skip to the start of the navMap
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (tok->IsStartTag() && tok->NameIsNS("navMap", EPUB_NCX_NS))
            break;
    }
    if (!tok || tok->IsError())
        return false;

    ScopedMem<WCHAR> itemText, itemSrc;
    int level = 0;
    while ((tok = parser.Next()) && !tok->IsError() && (!tok->IsEndTag() || !tok->NameIsNS("navMap", EPUB_NCX_NS))) {
        if (tok->IsTag() && tok->NameIsNS("navPoint", EPUB_NCX_NS)) {
            if (itemText) {
                visitor->Visit(itemText, itemSrc, level);
                itemText.Set(NULL);
                itemSrc.Set(NULL);
            }
            if (tok->IsStartTag())
                level++;
            else if (tok->IsEndTag() && level > 0)
                level--;
        }
        else if (tok->IsStartTag() && tok->NameIsNS("text", EPUB_NCX_NS)) {
            if (!(tok = parser.Next()) || tok->IsError())
                break;
            if (tok->IsText())
                itemText.Set(FromHtmlUtf8(tok->s, tok->sLen));
        }
        else if (tok->IsTag() && !tok->IsEndTag() && tok->NameIsNS("content", EPUB_NCX_NS)) {
            AttrInfo *attrInfo = tok->GetAttrByName("src");
            if (attrInfo) {
                ScopedMem<char> src(str::DupN(attrInfo->val, attrInfo->valLen));
                src.Set(NormalizeURL(src, pagePath));
                itemSrc.Set(FromHtmlUtf8(src, str::Len(src)));
            }
        }
    }

    return true;
}

bool EpubDoc::ParseToc(EbookTocVisitor *visitor)
{
    if (!tocPath)
        return false;
    size_t tocDataLen;
    ScopedMem<char> tocData(zip.GetFileData(tocPath, &tocDataLen));
    if (!tocData)
        return false;

    ScopedMem<char> pagePath(str::conv::ToUtf8(tocPath));
    if (isNcxToc)
        return ParseNcxToc(tocData, tocDataLen, pagePath, visitor);
    return ParseNavToc(tocData, tocDataLen, pagePath, visitor);
}

bool EpubDoc::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        ZipFile zip(fileName);
        ScopedMem<char> mimetype(zip.GetFileData(L"mimetype"));
        if (!mimetype)
            return false;
        // trailing whitespace is allowed for the mimetype file
        for (size_t i = str::Len(mimetype); i > 0; i--) {
            if (!str::IsWs(mimetype[i-1]))
                break;
            mimetype[i-1] = '\0';
        }
        // a proper EPUB document has a "mimetype" file with content
        // "application/epub+zip" as the first entry in its ZIP structure
        /* cf. http://forums.fofou.org/sumatrapdf/topic?id=2599331
        if (!str::Eq(zip.GetFileName(0), L"mimetype"))
            return false; */
        return str::Eq(mimetype, "application/epub+zip") ||
               // also open renamed .ibooks files
               // cf. http://en.wikipedia.org/wiki/IBooks#Formats
               str::Eq(mimetype, "application/x-ibooks+zip");
    }
    return str::EndsWithI(fileName, L".epub");
}

EpubDoc *EpubDoc::CreateFromFile(const WCHAR *fileName)
{
    EpubDoc *doc = new EpubDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

EpubDoc *EpubDoc::CreateFromStream(IStream *stream)
{
    EpubDoc *doc = new EpubDoc(stream);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

/* ********** FictionBook ********** */

Fb2Doc::Fb2Doc(const WCHAR *fileName) : fileName(str::Dup(fileName)),
    stream(NULL), isZipped(false), hasToc(false) { }

Fb2Doc::Fb2Doc(IStream *stream) : fileName(NULL),
    stream(stream), isZipped(false), hasToc(false) {
    stream->AddRef();
}

Fb2Doc::~Fb2Doc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).base.data);
        free(images.At(i).id);
    }
    if (stream)
        stream->Release();
}

bool Fb2Doc::Load()
{
    size_t len;
    ScopedMem<char> data;
    if (!fileName && stream)
        data.Set((char *)GetDataFromStream(stream, &len));
    else if (str::EndsWithI(fileName, L".zip") ||
        str::EndsWithI(fileName, L".fb2z") ||
        str::EndsWithI(fileName, L".zfb2")) {
        ZipFile archive(fileName);
        data.Set(archive.GetFileData((size_t)0, &len));
        isZipped = true;
    }
    else
        data.Set(file::ReadAll(fileName, &len));
    if (!data)
        return false;

    const char *xmlPI = str::Find(data, "<?xml");
    if (xmlPI) {
        UINT cp = GetCodepageFromPI(xmlPI);
        if (cp != CP_ACP && cp != CP_UTF8) {
            data.Set(str::ToMultiByte(data, cp, CP_UTF8));
            len = str::Len(data);
        }
    }

    HtmlPullParser parser(data, len);
    HtmlToken *tok;
    int inBody = 0, inTitleInfo = 0;
    const char *bodyStart = NULL;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (!inTitleInfo && tok->IsStartTag() && Tag_Body == tok->tag) {
            if (!inBody++)
                bodyStart = tok->s;
        }
        else if (inBody && tok->IsEndTag() && Tag_Body == tok->tag) {
            if (!--inBody) {
                if (xmlData.Count() > 0)
                    xmlData.Append("<pagebreak />");
                xmlData.Append('<');
                xmlData.Append(bodyStart, tok->s - bodyStart + tok->sLen);
                xmlData.Append('>');
            }
        }
        else if (inBody)
            continue;
        else if (inTitleInfo && tok->IsEndTag() && tok->NameIs("title-info"))
            inTitleInfo--;
        else if (inTitleInfo && tok->IsStartTag() && tok->NameIs("book-title")) {
            if (!(tok = parser.Next()) || tok->IsError())
                break;
            if (tok->IsText()) {
                ScopedMem<char> tmp(str::DupN(tok->s, tok->sLen));
                docTitle.Set(DecodeHtmlEntitites(tmp, CP_UTF8));
            }
        }
        else if (inTitleInfo && tok->IsStartTag() && tok->NameIs("author")) {
            while ((tok = parser.Next()) && !tok->IsError() &&
                !(tok->IsEndTag() && tok->NameIs("author"))) {
                if (tok->IsText()) {
                    ScopedMem<char> tmp(str::DupN(tok->s, tok->sLen));
                    ScopedMem<WCHAR> author(DecodeHtmlEntitites(tmp, CP_UTF8));
                    if (docAuthor)
                        docAuthor.Set(str::Join(docAuthor, L" ", author));
                    else
                        docAuthor.Set(author.StealData());
                }
            }
            if (docAuthor)
                str::NormalizeWS(docAuthor);
        }
        else if (inTitleInfo && tok->IsStartTag() && tok->NameIs("coverpage")) {
            tok = parser.Next();
            if (tok && tok->IsText())
                tok = parser.Next();
            if (tok && tok->IsEmptyElementEndTag() && Tag_Image == tok->tag) {
                AttrInfo *attr = tok->GetAttrByNameNS("href", "http://www.w3.org/1999/xlink");
                if (attr)
                    coverImage.Set(str::DupN(attr->val, attr->valLen));
            }
        }
        else if (inTitleInfo)
            continue;
        else if (tok->IsStartTag() && tok->NameIs("title-info"))
            inTitleInfo++;
        else if (tok->IsStartTag() && tok->NameIs("binary"))
            ExtractImage(&parser, tok);
    }

    return xmlData.Size() > 0;
}

void Fb2Doc::ExtractImage(HtmlPullParser *parser, HtmlToken *tok)
{
    ScopedMem<char> id;
    AttrInfo *attrInfo = tok->GetAttrByName("id");
    if (attrInfo)
        id.Set(str::DupN(attrInfo->val, attrInfo->valLen));

    tok = parser->Next();
    if (!tok || !tok->IsText())
        return;

    ImageData2 data = { 0 };
    data.base.data = Base64Decode(tok->s, tok->s + tok->sLen, &data.base.len);
    if (!data.base.data)
        return;
    data.id = str::Join("#", id);
    data.idx = images.Count();
    images.Append(data);
}

const char *Fb2Doc::GetTextData(size_t *lenOut)
{
    *lenOut = xmlData.Size();
    return xmlData.Get();
}

size_t Fb2Doc::GetTextDataSize()
{
    return xmlData.Size();
}

ImageData *Fb2Doc::GetImageData(const char *id)
{
    for (size_t i = 0; i < images.Count(); i++) {
        if (str::Eq(images.At(i).id, id))
            return &images.At(i).base;
    }
    return NULL;
}

ImageData *Fb2Doc::GetCoverImage()
{
    if (!coverImage)
        return NULL;
    return GetImageData(coverImage);
}

WCHAR *Fb2Doc::GetProperty(DocumentProperty prop) const
{
    if (Prop_Title == prop && docTitle)
        return str::Dup(docTitle);
    if (Prop_Author == prop && docAuthor)
        return str::Dup(docAuthor);
    return NULL;
}

const WCHAR *Fb2Doc::GetFileName() const
{
    return fileName;
}

bool Fb2Doc::IsZipped() const
{
    return isZipped;
}

bool Fb2Doc::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, L".fb2")  ||
           str::EndsWithI(fileName, L".fb2z") ||
           str::EndsWithI(fileName, L".zfb2") ||
           str::EndsWithI(fileName, L".fb2.zip");
}

Fb2Doc *Fb2Doc::CreateFromFile(const WCHAR *fileName)
{
    Fb2Doc *doc = new Fb2Doc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

Fb2Doc *Fb2Doc::CreateFromStream(IStream *stream)
{
    Fb2Doc *doc = new Fb2Doc(stream);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

/* ********** PalmDOC (and TealDoc) ********** */

PalmDoc::PalmDoc(const WCHAR *fileName) : fileName(str::Dup(fileName)) { }

PalmDoc::~PalmDoc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).base.data);
        free(images.At(i).id);
    }
}

#define PDB_TOC_ENTRY_MARK "ToC!Entry!"

// cf. http://wiki.mobileread.com/wiki/TealDoc
static const char *HandleTealDocTag(str::Str<char>& builder, WStrVec& tocEntries, const char *text, size_t len, UINT codePage)
{
    if (len < 9) {
Fallback:
        builder.Append("&lt;");
        return text;
    }
    if (!str::StartsWithI(text, "<BOOKMARK") &&
        !str::StartsWithI(text, "<HEADER") &&
        !str::StartsWithI(text, "<HRULE") &&
        !str::StartsWithI(text, "<LABEL") &&
        !str::StartsWithI(text, "<LINK") &&
        !str::StartsWithI(text, "<TEALPAINT")) {
        goto Fallback;
    }
    HtmlPullParser parser(text, len);
    HtmlToken *tok = parser.Next();
    if (!tok || !tok->IsStartTag())
        goto Fallback;

    if (tok->NameIs("BOOKMARK")) {
        // <BOOKMARK NAME="Contents">
        AttrInfo *attr = tok->GetAttrByName("NAME");
        if (attr && attr->valLen > 0) {
            tocEntries.Append(FromHtmlUtf8(attr->val, attr->valLen));
            builder.AppendFmt("<a name=" PDB_TOC_ENTRY_MARK "%d>", tocEntries.Count());
            return tok->s + tok->sLen;
        }
    }
    else if (tok->NameIs("HEADER")) {
        // <HEADER TEXT="Contents" ALIGN=CENTER STYLE=UNDERLINE>
        int hx = 2;
        AttrInfo *attr = tok->GetAttrByName("FONT");
        if (attr && attr->valLen > 0)
            hx = '0' == *attr->val ? 5 : '2' == *attr->val ? 1 : 3;
        attr = tok->GetAttrByName("TEXT");
        if (attr) {
            builder.AppendFmt("<h%d>", hx);
            builder.Append(attr->val, attr->valLen);
            builder.AppendFmt("</h%d>", hx);
            return tok->s + tok->sLen;
        }
    }
    else if (tok->NameIs("HRULE")) {
        // <HRULE STYLE=OUTLINE>
        builder.Append("<hr>");
        return tok->s + tok->sLen;
    }
    else if (tok->NameIs("LABEL")) {
        // <LABEL NAME="Contents">
        AttrInfo *attr = tok->GetAttrByName("NAME");
        if (attr && attr->valLen > 0) {
            builder.Append("<a name=\"");
            builder.Append(attr->val, attr->valLen);
            builder.Append("\">");
            return tok->s + tok->sLen;
        }
    }
    else if (tok->NameIs("LINK")) {
        // <LINK TEXT="Press Me" TAG="Contents" FILE="My Novels">
        AttrInfo *attrTag = tok->GetAttrByName("TAG");
        AttrInfo *attrText = tok->GetAttrByName("TEXT");
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
    }
    else if (tok->NameIs("TEALPAINT")) {
        // <TEALPAINT SRC="Pictures" INDEX=0 LINK=SUPERMAP SUPERIMAGE=1 SUPERW=640 SUPERH=480>
        // support removed in r7047
        return tok->s + tok->sLen;
    }
    goto Fallback;
}

bool PalmDoc::Load()
{
    MobiDoc *mobiDoc = MobiDoc::CreateFromFile(fileName);
    if (!mobiDoc)
        return false;
    if (Pdb_PalmDoc != mobiDoc->GetDocType() && Pdb_TealDoc != mobiDoc->GetDocType()) {
        delete mobiDoc;
        return false;
    }

    size_t textLen;
    const char *text = mobiDoc->GetBookHtmlData(textLen);
    UINT codePage = GuessTextCodepage((char *)text, textLen, CP_ACP);
    ScopedMem<char> textUtf8(str::ToMultiByte(text, codePage, CP_UTF8));
    textLen = str::Len(textUtf8);

    for (const char *curr = textUtf8; curr < textUtf8 + textLen; curr++) {
        if ('&' == *curr)
            htmlData.Append("&amp;");
        else if ('<' == *curr)
            curr = HandleTealDocTag(htmlData, tocEntries, curr, textUtf8 + textLen - curr, codePage);
        else if ('\n' == *curr || '\r' == *curr && curr + 1 < textUtf8 + textLen && '\n' != *(curr + 1))
            htmlData.Append("\n<br>");
        else
            htmlData.Append(*curr);
    }

    delete mobiDoc;
    return true;
}

const char *PalmDoc::GetTextData(size_t *lenOut)
{
    *lenOut = htmlData.Size();
    return htmlData.Get();
}

WCHAR *PalmDoc::GetProperty(DocumentProperty prop) const
{
    return NULL;
}

const WCHAR *PalmDoc::GetFileName() const
{
    return fileName;
}

bool PalmDoc::HasToc() const
{
    return tocEntries.Count() > 0;
}

bool PalmDoc::ParseToc(EbookTocVisitor *visitor)
{
    for (size_t i = 0; i < tocEntries.Count(); i++) {
        ScopedMem<WCHAR> name(str::Format(TEXT(PDB_TOC_ENTRY_MARK) L"%d", i + 1));
        visitor->Visit(tocEntries.At(i), name, 1);
    }
    return true;
}

bool PalmDoc::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        PdbReader pdbReader(fileName);
        return str::Eq(pdbReader.GetDbType(), "TEXtREAd") ||
               str::Eq(pdbReader.GetDbType(), "TEXtTlDc");
    }

    return str::EndsWithI(fileName, L".pdb");
}

PalmDoc *PalmDoc::CreateFromFile(const WCHAR *fileName)
{
    PalmDoc *doc = new PalmDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

/* ********** TCR (Text Compression for (Psion) Reader) ********** */

// cf. http://www.cix.co.uk/~gidds/Software/TCR.html
#define TCR_HEADER      "!!8-Bit!!"
#define TCR_HEADER_LEN  strlen(TCR_HEADER)

TcrDoc::TcrDoc(const WCHAR *fileName) : fileName(str::Dup(fileName)) { }
TcrDoc::~TcrDoc() { }

bool TcrDoc::Load()
{
    size_t dataLen;
    ScopedMem<char> data(file::ReadAll(fileName, &dataLen));
    if (!data)
        return false;
    if (dataLen < TCR_HEADER_LEN || !str::StartsWith(data.Get(), TCR_HEADER))
        return false;

    const char *curr = data + TCR_HEADER_LEN;
    const char *end = data + dataLen;

    const char *dict[256];
    for (int n = 0; n < dimof(dict); n++) {
        if (curr >= end)
            return false;
        dict[n] = curr;
        curr += 1 + (uint8_t)*curr;
    }

    str::Str<char> text(dataLen * 2);
    for (; curr < end; curr++) {
        const char *entry = dict[(uint8_t)*curr];
        text.Append(entry + 1, (uint8_t)*entry);
    }

    ScopedMem<char> textUtf8(DecodeTextToUtf8(text.Get()));
    if (!textUtf8)
        return false;
    htmlData.Append("<pre>");
    for (curr = textUtf8; *curr; curr++) {
        AppendChar(htmlData, *curr);
    }
    htmlData.Append("</pre>");

    return true;
}

const char *TcrDoc::GetTextData(size_t *lenOut)
{
    *lenOut = htmlData.Size();
    return htmlData.Get();
}

WCHAR *TcrDoc::GetProperty(DocumentProperty prop) const
{
    return NULL;
}

const WCHAR *TcrDoc::GetFileName() const
{
    return fileName;
}

bool TcrDoc::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff)
        return file::StartsWith(fileName, TCR_HEADER);

    return str::EndsWithI(fileName, L".tcr");
}

TcrDoc *TcrDoc::CreateFromFile(const WCHAR *fileName)
{
    TcrDoc *doc = new TcrDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

/* ********** Plain HTML ********** */

HtmlDoc::HtmlDoc(const WCHAR *fileName) : fileName(str::Dup(fileName)) { }

HtmlDoc::~HtmlDoc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).base.data);
        free(images.At(i).id);
    }
}

bool HtmlDoc::Load()
{
    ScopedMem<char> data(file::ReadAll(fileName, NULL));
    if (!data)
        return false;
    if (str::StartsWith(data.Get(), "<?xml")) {
        UINT cp = GetCodepageFromPI(data);
        if (cp != CP_ACP)
            htmlData.Set(str::ToMultiByte(data, cp, CP_UTF8));
    }
    if (!htmlData)
        htmlData.Set(DecodeTextToUtf8(data));
    if (!htmlData)
        return false;

    pagePath.Set(str::conv::ToUtf8(fileName));
    str::TransChars(pagePath, "\\", "/");

    HtmlPullParser parser(htmlData, str::Len(htmlData));
    HtmlToken *tok;
    while ((tok = parser.Next()) && !tok->IsError() &&
        (!tok->IsTag() || Tag_Body != tok->tag && Tag_P != tok->tag)) {
        if (tok->IsStartTag() && Tag_Title == tok->tag) {
            tok = parser.Next();
            if (tok && tok->IsText())
                title.Set(FromHtmlUtf8(tok->s, tok->sLen));
        }
        else if ((tok->IsStartTag() || tok->IsEmptyElementEndTag()) && Tag_Meta == tok->tag) {
            AttrInfo *attrName = tok->GetAttrByName("name");
            AttrInfo *attrValue = tok->GetAttrByName("content");
            if (!attrName || !attrValue)
                /* ignore this tag */;
            else if (attrName->ValIs("author"))
                author.Set(FromHtmlUtf8(attrValue->val, attrValue->valLen));
            else if (attrName->ValIs("date"))
                date.Set(FromHtmlUtf8(attrValue->val, attrValue->valLen));
            else if (attrName->ValIs("copyright"))
                copyright.Set(FromHtmlUtf8(attrValue->val, attrValue->valLen));
        }
    }

    return true;
}

const char *HtmlDoc::GetTextData(size_t *lenOut)
{
    *lenOut = htmlData ? str::Len(htmlData) : 0;
    return htmlData;
}

ImageData *HtmlDoc::GetImageData(const char *id)
{
    ScopedMem<char> url(NormalizeURL(id, pagePath));
    str::UrlDecodeInPlace(url);
    for (size_t i = 0; i < images.Count(); i++) {
        if (str::Eq(images.At(i).id, url))
            return &images.At(i).base;
    }

    ScopedMem<WCHAR> path(str::conv::FromUtf8(url));
    str::TransChars(path, L"/", L"\\");
    ImageData2 data = { 0 };
    data.base.data = file::ReadAll(path, &data.base.len);
    if (!data.base.data)
        return NULL;
    data.id = url.StealData();
    images.Append(data);
    return &images.Last().base;
}

char *HtmlDoc::GetFileData(const char *relPath, size_t *lenOut)
{
    ScopedMem<char> url(NormalizeURL(relPath, pagePath));
    str::UrlDecodeInPlace(url);
    ScopedMem<WCHAR> path(str::conv::FromUtf8(url));
    str::TransChars(path, L"/", L"\\");
    return file::ReadAll(path, lenOut);
}

WCHAR *HtmlDoc::GetProperty(DocumentProperty prop) const
{
    switch (prop) {
    case Prop_Title:
        return str::Dup(title);
    case Prop_Author:
        return str::Dup(author);
    case Prop_CreationDate:
        return str::Dup(date);
    case Prop_Copyright:
        return str::Dup(copyright);
    default:
        return NULL;
    }
}

const WCHAR *HtmlDoc::GetFileName() const
{
    return fileName;
}

bool HtmlDoc::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, L".html") ||
           str::EndsWithI(fileName, L".htm") ||
           str::EndsWithI(fileName, L".xhtml");
}

HtmlDoc *HtmlDoc::CreateFromFile(const WCHAR *fileName)
{
    HtmlDoc *doc = new HtmlDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

/* ********** Plain Text ********** */

TxtDoc::TxtDoc(const WCHAR *fileName) : fileName(str::Dup(fileName)), isRFC(false) { }

static char *TextFindLinkEnd(str::Str<char>& htmlData, char *curr, bool atStart, bool fromWww=false)
{
    char *end = curr;
    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    for (; *end && !str::IsWs(*end); end++);
    if (',' == end[-1] || '.' == end[-1] || '?' == end[-1] || '!' == end[-1])
        end--;
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == end[-1] && (!str::FindChar(curr, '(') || str::FindChar(curr, '(') >= end))
        end--;
    // cut the link at the first double quote if it's also preceded by one
    if (!atStart && curr[-1] == '"' && str::FindChar(curr, '"') && str::FindChar(curr, '"') < end)
        end = (char *)str::FindChar(curr, '"');

    if (fromWww && (end - curr <= 4 || !str::FindChar(curr + 5, '.') ||
                    str::FindChar(curr + 5, '.') >= end)) {
        // ignore www. links without a top-level domain
        return NULL;
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
inline bool IsEmailUsernameChar(char c)
{
    // explicitly excluding the '/' from the list, as it is more
    // often part of a URL or path than of an email address
    return isalnum((unsigned char)c) || c && str::FindChar(".!#$%&'*+=?^_`{|}~-", c);
}
inline bool IsEmailDomainChar(char c)
{
    return isalnum((unsigned char)c) || '-' == c;
}

static char *TextFindEmailEnd(str::Str<char>& htmlData, char *curr, bool fromAt=false)
{
    ScopedMem<char> beforeAt;
    char *end = curr;
    if (fromAt) {
        if (htmlData.Count() == 0 || !IsEmailUsernameChar(htmlData.Last()))
            return NULL;
        size_t idx = htmlData.Count();
        for (; idx > 1 && IsEmailUsernameChar(htmlData.At(idx - 1)); idx--);
        beforeAt.Set(str::Dup(&htmlData.At(idx)));
        htmlData.RemoveAt(idx, htmlData.Count() - idx);
    } else {
        end = curr + 7; // skip mailto:
        if (!IsEmailUsernameChar(*end))
            return NULL;
        for (; IsEmailUsernameChar(*end); end++);
    }

    if (*end != '@' || !IsEmailDomainChar(*(end + 1)))
        return NULL;
    for (end++; IsEmailDomainChar(*end); end++);
    if ('.' != *end || !IsEmailDomainChar(*(end + 1)))
        return NULL;
    do {
        for (end++; IsEmailDomainChar(*end); end++);
    } while ('.' == *end && IsEmailDomainChar(*(end + 1)));

    htmlData.Append("<a href=\"");
    if (fromAt)
        htmlData.AppendFmt("mailto:%s", beforeAt);
    for (; curr < end; curr++) {
        AppendChar(htmlData, *curr);
    }
    htmlData.Append("\">");
    if (fromAt)
        htmlData.Append(beforeAt);

    return end;
}

static char *TextFindRfcEnd(str::Str<char>& htmlData, char *curr)
{
    if (isalnum((unsigned char)*(curr - 1)))
        return NULL;
    int rfc;
    char *end = (char *)str::Parse(curr, "RFC %d", &rfc);
    // cf. http://en.wikipedia.org/wiki/Request_for_Comments#Obtaining_RFCs
    htmlData.AppendFmt("<a href='http://www.rfc-editor.org/rfc/rfc%d.txt'>", rfc);
    return end;
}

bool TxtDoc::Load()
{
    ScopedMem<char> text(file::ReadAll(fileName, NULL));
    text.Set(DecodeTextToUtf8(text));
    if (!text)
        return false;

    int rfc;
    isRFC = str::Parse(path::GetBaseName(fileName), L"rfc%d.txt%$", &rfc) != NULL;

    char *curr = text;
    if (str::StartsWith(text.Get(), UTF8_BOM))
        curr += 3;
    char *linkEnd = NULL;
    bool rfcHeader = false;
    int sectionCount = 0;

    htmlData.Append("<pre>");
    while (*curr) {
        // similar logic to LinkifyText in PdfEngine.cpp
        if (linkEnd == curr) {
            htmlData.Append("</a>");
            linkEnd = NULL;
        }
        else if (linkEnd)
            /* don't check for hyperlinks inside a link */;
        else if ('@' == *curr)
            linkEnd = TextFindEmailEnd(htmlData, curr, true);
        else if (curr > text && ('/' == curr[-1] || isalnum((unsigned char)curr[-1])))
            /* don't check for a link at this position */;
        else if ('h' == *curr && str::Parse(curr, "http%?s://"))
            linkEnd = TextFindLinkEnd(htmlData, curr, curr == text);
        else if ('w' == *curr && str::StartsWith(curr, "www."))
            linkEnd = TextFindLinkEnd(htmlData, curr, curr == text, true);
        else if ('m' == *curr && str::StartsWith(curr, "mailto:"))
            linkEnd = TextFindEmailEnd(htmlData, curr);
        else if (isRFC && curr > text && 'R' == *curr && str::Parse(curr, "RFC %d", &rfc))
            linkEnd = TextFindRfcEnd(htmlData, curr);

        // RFCs use (among others) form feeds as page separators
        if ('\f' == *curr && (curr == text || '\n' == *(curr - 1)) &&
            (!*(curr + 1) || '\r' == *(curr + 1) || '\n' == *(curr + 1))) {
            // only insert pagebreaks if not at the very beginning or end
            if (curr > text && *(curr + 2) && (*(curr + 3) || *(curr + 2) != '\n'))
                htmlData.Append("<pagebreak />");
            curr++;
            continue;
        }

        if (isRFC && curr > text && '\n' == *(curr - 1) &&
            (str::IsDigit(*curr) || str::StartsWith(curr, "APPENDIX")) &&
            str::FindChar(curr, '\n') && str::Parse(str::FindChar(curr, '\n') + 1, "%?\r\n")) {
            htmlData.AppendFmt("<b id='section%d' title=\"", ++sectionCount);
            for (const char *c = curr; *c != '\r' && *c != '\n'; c++) {
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
        curr++;
    }
    if (linkEnd)
        htmlData.Append("</a>");
    htmlData.Append("</pre>");
    
    return true;
}

const char *TxtDoc::GetTextData(size_t *lenOut)
{
    *lenOut = htmlData.Size();
    return htmlData.Get();
}

WCHAR *TxtDoc::GetProperty(DocumentProperty prop) const
{
    return NULL;
}

const WCHAR *TxtDoc::GetFileName() const
{
    return fileName;
}

bool TxtDoc::IsRFC() const
{
    return isRFC;
}

bool TxtDoc::HasToc() const
{
    return isRFC;
}

bool TxtDoc::ParseToc(EbookTocVisitor *visitor)
{
    if (!isRFC)
        return false;

    HtmlParser parser;
    parser.Parse(htmlData.Get(), CP_UTF8);
    HtmlElement *el = NULL;
    while ((el = parser.FindElementByName("b", el))) {
        ScopedMem<WCHAR> title(el->GetAttribute("title"));
        ScopedMem<WCHAR> id(el->GetAttribute("id"));
        int level = 1;
        if (str::IsDigit(*title)) {
            const WCHAR *dot = str::FindChar(title, '.');
            while ((dot = str::FindChar(dot + 1, '.'))) {
                level++;
            }
        }
        visitor->Visit(title, id, level);
    }

    return true;
}

bool TxtDoc::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, L".txt") ||
           str::EndsWithI(fileName, L".log") ||
           // http://en.wikipedia.org/wiki/.nfo
           str::EndsWithI(fileName, L".nfo") ||
           // http://en.wikipedia.org/wiki/FILE_ID.DIZ
           str::EndsWithI(fileName, L"\\file_id.diz") ||
           // http://en.wikipedia.org/wiki/Read.me
           str::EndsWithI(fileName, L"\\Read.me");
}

TxtDoc *TxtDoc::CreateFromFile(const WCHAR *fileName)
{
    TxtDoc *doc = new TxtDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}
