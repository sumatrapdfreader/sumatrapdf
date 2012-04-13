/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EpubDoc.h"
#include "FileUtil.h"
#include "HtmlPullParser.h"
#include "TrivialHtmlParser.h"
#include "WinUtil.h"
#include "ZipUtil.h"

inline TCHAR *FromHtmlUtf8(const char *s, size_t len)
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

EpubDoc::EpubDoc(const TCHAR *fileName) :
    zip(fileName), fileName(str::Dup(fileName)) { }

EpubDoc::EpubDoc(IStream *stream) :
    zip(stream), fileName(NULL) { }

EpubDoc::~EpubDoc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).base.data);
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
                char *text = ResolveHtmlEntities(tok->s, tok->sLen);
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

size_t EpubDoc::GetTextDataSize()
{
    return htmlData.Size();
}

ImageData *EpubDoc::GetImageData(const char *id, const char *pagePath)
{
    ScopedMem<char> url(NormalizeURL(id, pagePath));
    for (size_t i = 0; i < images.Count(); i++) {
        if (str::Eq(images.At(i).id, url)) {
            if (!images.At(i).base.data)
                images.At(i).base.data = zip.GetFileData(images.At(i).idx, &images.At(i).base.len);
            if (images.At(i).base.data)
                return &images.At(i).base;
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

const TCHAR *EpubDoc::GetFileName() const
{
    return fileName;
}

bool EpubDoc::HasToc() const
{
    return tocPath != NULL;
}

bool EpubDoc::ParseToc(EbookTocVisitor *visitor)
{
    if (!tocPath)
        return false;
    size_t tocDataLen;
    ScopedMem<char> tocData(zip.GetFileData(tocPath, &tocDataLen));
    if (!tocData)
        return false;

    HtmlPullParser parser(tocData, tocDataLen);
    HtmlToken *tok;
    // skip to the start of the navMap
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (tok->IsStartTag() && (tok->NameIs("navMap") || tok->NameIs("ncx:navMap")))
            break;
    }
    if (!tok || tok->IsError())
        return false;

    ScopedMem<TCHAR> itemText, itemSrc;
    int level = 0;

    while ((tok = parser.Next()) && !tok->IsError() && (!tok->IsEndTag() || !tok->NameIs("navMap") && !tok->NameIs("ncx:navMap"))) {
        if (tok->IsTag() && (tok->NameIs("navPoint") || tok->NameIs("ncx:navPoint"))) {
            if (itemText) {
                visitor->visit(itemText, itemSrc, level);
                itemText.Set(NULL);
                itemSrc.Set(NULL);
            }
            if (tok->IsStartTag())
                level++;
            else if (tok->IsEndTag())
                level--;
        }
        else if (tok->IsStartTag() && (tok->NameIs("text") || tok->NameIs("ncx:text"))) {
            tok = parser.Next();
            if (tok->IsText())
                itemText.Set(FromHtmlUtf8(tok->s, tok->sLen));
            else if (tok->IsError())
                break;
        }
        else if (tok->IsTag() && !tok->IsEndTag() && (tok->NameIs("content") || tok->NameIs("ncx:content"))) {
            AttrInfo *attrInfo = tok->GetAttrByName("src");
            if (attrInfo)
                itemSrc.Set(FromHtmlUtf8(attrInfo->val, attrInfo->valLen));
        }
    }

    return true;
}

bool EpubDoc::VerifyEpub(ZipFile& zip)
{
    ScopedMem<char> mimetype(zip.GetFileData(_T("mimetype")));
    if (!mimetype)
        return false;
    // trailing whitespace is allowed for the mimetype file
    for (size_t i = str::Len(mimetype); i > 0; i--) {
        if (!str::IsWs(mimetype[i-1]))
            break;
        mimetype[i-1] = '\0';
    }
    // a proper EPUB documents has a "mimetype" file with content
    // "application/epub+zip" as the first entry in its ZIP structure
    /* cf. http://forums.fofou.org/sumatrapdf/topic?id=2599331
    if (!str::Eq(zip.GetFileName(0), _T("mimetype")))
        return false; */
    return str::Eq(mimetype, "application/epub+zip");
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

Fb2Doc::Fb2Doc(const TCHAR *fileName) : fileName(str::Dup(fileName)),
    isZipped(false), hasToc(false) { }

Fb2Doc::~Fb2Doc()
{
    for (size_t i = 0; i < images.Count(); i++) {
        free(images.At(i).base.data);
        free(images.At(i).id);
    }
}

bool Fb2Doc::Load()
{
    size_t len;
    ScopedMem<char> data;
    if (str::EndsWithI(fileName, _T(".zip")) ||
        str::EndsWithI(fileName, _T(".fb2z")) ||
        str::EndsWithI(fileName, _T(".zfb2"))) {
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
        if (!inBody && !inTitleInfo && tok->IsStartTag() && tok->NameIs("FictionBook") &&
            !hrefName && parser.tagNesting.Count() == 1) {
            AttrInfo *attr = tok->GetAttrByValue("http://www.w3.org/1999/xlink");
            if (attr && attr->nameLen > 6 && str::StartsWith(attr->name, "xmlns:")) {
                ScopedMem<char> ns(str::DupN(attr->name + 6, attr->nameLen - 6));
                hrefName.Set(str::Join(ns, ":href"));
            }
        }
        else if (!inTitleInfo && tok->IsStartTag() && Tag_Body == tok->tag) {
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
            tok = parser.Next();
            if (tok->IsText()) {
                ScopedMem<char> tmp(str::DupN(tok->s, tok->sLen));
                docTitle.Set(DecodeHtmlEntitites(tmp, CP_UTF8));
            }
            else if (tok->IsError())
                break;
        }
        else if (inTitleInfo && tok->IsStartTag() && tok->NameIs("author")) {
            while ((tok = parser.Next()) && !tok->IsError() &&
                !(tok->IsEndTag() && tok->NameIs("author"))) {
                if (tok->IsText()) {
                    ScopedMem<char> tmp(str::DupN(tok->s, tok->sLen));
                    ScopedMem<TCHAR> author(DecodeHtmlEntitites(tmp, CP_UTF8));
                    if (docAuthor)
                        docAuthor.Set(str::Join(docAuthor, _T(" "), author));
                    else
                        docAuthor.Set(author.StealData());
                }
            }
            if (docAuthor)
                str::NormalizeWS(docAuthor);
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

ImageData *Fb2Doc::GetImageData(const char *id)
{
    for (size_t i = 0; i < images.Count(); i++) {
        if (str::Eq(images.At(i).id, id))
            return &images.At(i).base;
    }
    return NULL;
}

TCHAR *Fb2Doc::GetProperty(const char *name)
{
    if (str::Eq(name, "Title") && docTitle)
        return str::Dup(docTitle);
    if (str::Eq(name, "Author") && docAuthor)
        return str::Dup(docAuthor);
    return NULL;
}

const TCHAR *Fb2Doc::GetFileName() const
{
    return fileName;
}

const char *Fb2Doc::GetHrefName() const
{
    return hrefName ? hrefName : "href";
}

bool Fb2Doc::IsZipped() const
{
    return isZipped;
}

bool Fb2Doc::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, _T(".fb2"))  ||
           str::EndsWithI(fileName, _T(".fb2z")) ||
           str::EndsWithI(fileName, _T(".zfb2")) ||
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

/* HTML */

HtmlDoc::HtmlDoc(const TCHAR *fileName) : fileName(str::Dup(fileName)) { }

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
    for (size_t i = 0; i < images.Count(); i++) {
        if (str::Eq(images.At(i).id, url))
            return &images.At(i).base;
    }

    ScopedMem<TCHAR> path(str::conv::FromUtf8(url));
    str::TransChars(path, _T("/"), _T("\\"));
    ImageData2 data = { 0 };
    data.base.data = file::ReadAll(path, &data.base.len);
    if (!data.base.data)
        return NULL;
    data.id = url.StealData();
    images.Append(data);
    return &images.Last().base;
}

const TCHAR *HtmlDoc::GetFileName() const
{
    return fileName;
}

bool HtmlDoc::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, _T(".html")) ||
           str::EndsWithI(fileName, _T(".htm")) ||
           str::EndsWithI(fileName, _T(".xhtml"));
}

HtmlDoc *HtmlDoc::CreateFromFile(const TCHAR *fileName)
{
    HtmlDoc *doc = new HtmlDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}

/* Plain Text */

TxtDoc::TxtDoc(const TCHAR *fileName) : fileName(str::Dup(fileName)), isRFC(false) { }

static inline void AppendChar(str::Str<char>& htmlData, char c)
{
    switch (c) {
    case '&': htmlData.Append("&amp;"); break;
    case '<': htmlData.Append("&lt;"); break;
    case '"': htmlData.Append("&quot;"); break;
    default:  htmlData.Append(c); break;
    }
}

static char *TextFindLinkEnd(str::Str<char>& htmlData, char *curr, bool fromWww=false)
{
    char *end = curr;
    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    for (; *end && !str::IsWs(*end); end++);
    if (',' == end[-1] || '.' == end[-1])
        end--;
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == end[-1] && (!str::FindChar(curr, '(') || str::FindChar(curr, '(') >= end))
        end--;

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
    return isalnum((unsigned char)c) || c && str::FindChar(".!#$%&'*+-=?^`{|}~", c);
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
    if (isalnum((unsigned)*(curr - 1)))
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
    isRFC = str::Parse(path::GetBaseName(fileName), _T("rfc%d.txt%$"), &rfc) != NULL;

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
            linkEnd = TextFindLinkEnd(htmlData, curr);
        else if ('w' == *curr && str::StartsWith(curr, "www."))
            linkEnd = TextFindLinkEnd(htmlData, curr, true);
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

const TCHAR *TxtDoc::GetFileName() const
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
        ScopedMem<TCHAR> title(el->GetAttribute("title"));
        ScopedMem<TCHAR> id(el->GetAttribute("id"));
        int level = 1;
        if (str::IsDigit(*title)) {
            const TCHAR *dot = str::FindChar(title, '.');
            while ((dot = str::FindChar(dot + 1, '.'))) {
                level++;
            }
        }
        visitor->visit(title, id, level);
    }

    return true;
}

bool TxtDoc::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return str::EndsWithI(fileName, _T(".txt")) ||
           str::EndsWithI(fileName, _T(".log")) ||
           // http://en.wikipedia.org/wiki/.nfo
           str::EndsWithI(fileName, _T(".nfo")) ||
           // http://en.wikipedia.org/wiki/FILE_ID.DIZ
           str::EndsWithI(fileName, _T("\\file_id.diz")) ||
           // http://en.wikipedia.org/wiki/Read.me
           str::EndsWithI(fileName, _T("\\Read.me"));
}

TxtDoc *TxtDoc::CreateFromFile(const TCHAR *fileName)
{
    TxtDoc *doc = new TxtDoc(fileName);
    if (!doc || !doc->Load()) {
        delete doc;
        return NULL;
    }
    return doc;
}
