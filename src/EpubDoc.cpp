/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EpubDoc.h"
#include "FileUtil.h"
#include "HtmlPullParser.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "TrivialHtmlParser.h"
#include "Vec.h"
#include "ZipUtil.h"

/* URL handling helpers */

char *NormalizeURL(const char *url, const char *base)
{
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

static void UrlDecode(TCHAR *url)
{
    for (TCHAR *src = url; *src; src++, url++) {
        int val;
        if (*src == '%' && str::Parse(src, _T("%%%2x"), &val)) {
            *url = (char)val;
            src += 2;
        } else {
            *url = *src;
        }
    }
    *url = '\0';
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
    char *result = SAZA(char, bound);
    char *curr = result;
    unsigned char c = 0;
    int step = 0;
    for (; s < end && *s != '='; s++) {
        char n = decode64(*s);
        if (-1 == n) {
            if (isspace(*s))
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

/* EPUB */

EpubDoc::EpubDoc(const TCHAR *fileName) : zip(fileName) { }
EpubDoc::EpubDoc(IStream *stream) : zip(stream) { }

EpubDoc::~EpubDoc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).data);
        free(images.At(i).id);
    }
    for (size_t i = 1; i < props.Count(); i += 2) {
        free((void *)props.At(i));
    }
}

bool EpubDoc::Load()
{
    if (!VerifyEpub(zip))
        return false;

    ScopedMem<char> container(zip.GetFileData(_T("META-INF/container.xml")));
    HtmlParser parser;
    HtmlElement *node = parser.ParseInPlace(container);
    if (!node)
        return false;
    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByName("rootfile");
    if (!node)
        return false;
    ScopedMem<TCHAR> contentPath(node->GetAttribute("full-path"));
    if (!contentPath)
        return false;

    ScopedMem<char> content(zip.GetFileData(contentPath));
    if (!content)
        return false;
    ParseMetadata(content);
    node = parser.ParseInPlace(content);
    if (!node)
        return false;
    node = parser.FindElementByName("manifest");
    if (!node)
        return false;

    if (str::FindChar(contentPath, '/'))
        *(TCHAR *)(str::FindCharLast(contentPath, '/') + 1) = '\0';
    else
        *contentPath = '\0';
    StrVec idPathMap;

    for (node = node->down; node; node = node->next) {
        ScopedMem<TCHAR> mediatype(node->GetAttribute("media-type"));
        if (str::Eq(mediatype, _T("application/xhtml+xml"))) {
            ScopedMem<TCHAR> htmlPath(node->GetAttribute("href"));
            ScopedMem<TCHAR> htmlId(node->GetAttribute("id"));
            if (htmlPath && htmlId) {
                idPathMap.Append(htmlId.StealData());
                idPathMap.Append(htmlPath.StealData());
            }
        }
        else if (str::Eq(mediatype, _T("image/png"))  ||
                 str::Eq(mediatype, _T("image/jpeg")) ||
                 str::Eq(mediatype, _T("image/gif"))) {
            ScopedMem<TCHAR> imgPath(node->GetAttribute("href"));
            if (!imgPath)
                continue;
            ScopedMem<TCHAR> zipPath(str::Join(contentPath, imgPath));
            UrlDecode(zipPath);
            // load the image lazily
            ImageData2 data = { 0 };
            data.id = str::conv::ToUtf8(imgPath);
            data.idx = zip.GetFileIndex(zipPath);
            images.Append(data);
        }
        else if (str::Eq(mediatype, _T("application/x-dtbncx+xml"))) {
            tocPath.Set(node->GetAttribute("href"));
            if (tocPath)
                tocPath.Set(str::Join(contentPath, tocPath));
        }
    }

    node = parser.FindElementByName("spine");
    if (!node)
        return false;
    for (node = node->down; node; node = node->next) {
        if (!str::Eq(node->name, "itemref"))
            continue;
        ScopedMem<TCHAR> idref(node->GetAttribute("idref"));
        if (!idref)
            continue;
        const TCHAR *htmlPath = NULL;
        for (size_t i = 0; i < idPathMap.Count() && !htmlPath; i += 2) {
            if (str::Eq(idref, idPathMap.At(i)))
                htmlPath = idPathMap.At(i+1);
        }
        if (!htmlPath)
            continue;

        ScopedMem<TCHAR> fullPath(str::Join(contentPath, htmlPath));
        ScopedMem<char> html(zip.GetFileData(fullPath));
        if (!html)
            continue;
        // insert explicit page-breaks between sections including
        // an anchor with the file name at the top (for internal links)
        ScopedMem<char> utf8_path(str::conv::ToUtf8(htmlPath));
        htmlData.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", utf8_path);
        htmlData.Append(html);
    }

    return htmlData.Count() > 0;
}

void EpubDoc::ParseMetadata(const char *content)
{
    const char *metadataMap[] = {
        "dc:title",         "Title",
        "dc:creator",       "Author",
        "dc:date",          "CreationDate",
        "dc:description",   "Subject",
        "dc:rights",        "Copyright",
    };

    HtmlPullParser pullParser(content, str::Len(content));
    int insideMetadata = 0;
    HtmlToken *tok;

    while ((tok = pullParser.Next())) {
        if (tok->IsStartTag() && tok->NameIs("metadata"))
            insideMetadata++;
        else if (tok->IsEndTag() && tok->NameIs("metadata"))
            insideMetadata--;
        if (!insideMetadata)
            continue;
        if (!tok->IsStartTag())
            continue;

        for (int i = 0; i < dimof(metadataMap); i += 2) {
            if (tok->NameIs(metadataMap[i])) {
                tok = pullParser.Next();
                if (!tok->IsText())
                    break;
                ScopedMem<const char> value(str::DupN(tok->s, tok->sLen));
                const char *text = ResolveHtmlEntities(value, value + tok->sLen, NULL);
                if (text == value)
                    text = str::Dup(text);
                if (text) {
                    props.Append(metadataMap[i+1]);
                    props.Append(text);
                }
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

ImageData2 *EpubDoc::GetImageData(const char *id, const char *pagePath)
{
    ScopedMem<char> url(NormalizeURL(id, pagePath));
    for (size_t i = 0; i < images.Count(); i++) {
        if (str::Eq(images.At(i).id, url)) {
            if (!images.At(i).data)
                images.At(i).data = zip.GetFileData(images.At(i).idx, &images.At(i).len);
            if (images.At(i).data)
                return &images.At(i);
        }
    }
    return NULL;
}

TCHAR *EpubDoc::GetProperty(const char *name)
{
    for (size_t i = 0; i < props.Count(); i += 2) {
        if (str::Eq(props.At(i), name))
            return str::conv::FromUtf8(props.At(i + 1));
    }
    return NULL;
}

char *EpubDoc::GetTocData()
{
    if (!tocPath)
        return NULL;
    return zip.GetFileData(tocPath);
}

bool EpubDoc::VerifyEpub(ZipFile& zip)
{
    ScopedMem<char> firstFileData(zip.GetFileData(_T("mimetype")));
    // a proper EPUB documents has a "mimetype" file with content
    // "application/epub+zip" as the first entry in its ZIP structure
    return str::Eq(zip.GetFileName(0), _T("mimetype")) &&
           str::Eq(firstFileData, "application/epub+zip");
}

bool EpubDoc::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (sniff)
        return VerifyEpub(ZipFile(fileName));
    return str::EndsWithI(fileName, _T(".epub"));
}

EpubDoc *EpubDoc::CreateFromFile(const TCHAR *fileName)
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

/* FictionBook2 */

Fb2Doc::Fb2Doc(const TCHAR *fileName) : fileName(str::Dup(fileName)) { }

Fb2Doc::~Fb2Doc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).data);
        free(images.At(i).id);
    }
}

bool Fb2Doc::Load()
{
    size_t len;
    ScopedMem<char> data;
    if (str::EndsWithI(fileName, _T(".zip"))) {
        ZipFile archive(fileName);
        data.Set(archive.GetFileData((size_t)0, &len));
    }
    else {
        data.Set(file::ReadAll(fileName, &len));
    }
    if (!data)
        return false;

    const char *xmlPI = str::Find(data, "<?xml");
    if (xmlPI && str::Find(xmlPI, "?>")) {
        HtmlToken pi;
        pi.SetValue(HtmlToken::EmptyElementTag, xmlPI + 2, str::Find(xmlPI, "?>"));
        AttrInfo *enc = pi.GetAttrByName("encoding");
        if (enc) {
            ScopedMem<char> tmp(str::DupN(enc->val, enc->valLen));
            if (str::Find(tmp, "1251")) {
                data.Set(str::ToMultiByte(data, 1251, CP_UTF8));
                len = str::Len(data);
            }
        }
    }

    HtmlPullParser parser(data, len);
    HtmlToken *tok;
    int inBody = 0, inTitleInfo = 0;
    const char *bodyStart = NULL;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (!inTitleInfo && tok->IsStartTag() && tok->NameIs("body")) {
            if (!inBody++)
                bodyStart = tok->s;
        }
        else if (inBody && tok->IsEndTag() && tok->NameIs("body")) {
            if (!--inBody) {
                if (xmlData.Count() > 0)
                    xmlData.Append("<pagebreak />");
                xmlData.Append('<');
                xmlData.Append(bodyStart, tok->s - bodyStart + tok->sLen);
                xmlData.Append('>');
            }
        }
        else if (!inBody && tok->IsStartTag() && tok->NameIs("binary"))
            ExtractImage(&parser, tok);
        else if (!inBody && tok->IsStartTag() && tok->NameIs("title-info"))
            inTitleInfo++;
        else if (inTitleInfo && tok->IsEndTag() && tok->NameIs("title-info"))
            inTitleInfo--;
        else if (inTitleInfo && tok->IsStartTag() && tok->NameIs("book-title")) {
            tok = parser.Next();
            if (tok->IsText()) {
                ScopedMem<char> tmp(str::DupN(tok->s, tok->sLen));
                docTitle.Set(DecodeHtmlEntitites(tmp, CP_UTF8));
            }
            else if (tok->IsError())
                break;
        }
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
    data.data = Base64Decode(tok->s, tok->s + tok->sLen, &data.len);
    if (!data.data)
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

ImageData2 *Fb2Doc::GetImageData(const char *id)
{
    for (size_t i = 0; i < images.Count(); i++) {
        if (str::Eq(images.At(i).id, id))
            return &images.At(i);
    }
    return NULL;
}

TCHAR *Fb2Doc::GetProperty(const char *name)
{
    if (str::Eq(name, "Title") && docTitle)
        return str::Dup(docTitle);
    return NULL;
}

bool Fb2Doc::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, _T(".fb2")) ||
           str::EndsWithI(fileName, _T(".fb2.zip"));
}

Fb2Doc *Fb2Doc::CreateFromFile(const TCHAR *fileName)
{
    Fb2Doc *doc = new Fb2Doc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}
