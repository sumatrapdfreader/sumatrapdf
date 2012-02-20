/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

#include "BaseUtil.h"
#include "Vec.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "ZipUtil.h"
#include "HtmlPullParser.h"

#include "Fb2Doc.h"

/*
Basic FB2 format support:
- convert basic FB2 markup into HTML

TODO:
- convert links, images and further tags to HTML
- extract metadata
- extract basic document styles from stylesheet
*/

class Fb2DocImpl : public Fb2Doc {
    friend Fb2Doc;

    ScopedMem<TCHAR> fileName;
    str::Str<char> htmlData;
    Vec<ImageData2> images;

    bool Load();
    void ExtractImage(HtmlPullParser& parser, HtmlToken *tok);

public:
    Fb2DocImpl(const TCHAR *fileName) : fileName(str::Dup(fileName)) { }
    virtual ~Fb2DocImpl() {
        for (size_t i = 0; i < images.Count(); i++) {
            free(images.At(i).data);
            free(images.At(i).id);
        }
    }

    virtual const TCHAR *GetFilepath() {
        return fileName;
    }

    virtual const char *GetBookHtmlData(size_t& lenOut) {
        lenOut = htmlData.Size();
        return htmlData.Get();
    }

    virtual ImageData2 *GetImageData(const char *id) {
        for (size_t i = 0; i < images.Count(); i++) {
            if (str::Eq(images.At(i).id, id))
                return GetImageData(i);
        }
        return NULL;
    }

    virtual ImageData2 *GetImageData(size_t index) {
        if (index > images.Count())
            return NULL;
        return &images.At(index);
    }
};

bool Fb2DocImpl::Load()
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

    HtmlPullParser parser(data, len);
    HtmlToken *tok;
    int inBody = 0, inTitle = 0;
    int sectionDepth = 1;
    while ((tok = parser.Next())) {
        HtmlTag tag = tok->IsTag() ? FindTag(tok) : Tag_NotFound;
        if (!inBody && tok->IsStartTag() && Tag_Body == tag)
            inBody++;
        else if (inBody && tok->IsEndTag() && Tag_Body == tag)
            inBody--;
        if (!inBody && tok->IsStartTag() && tok->NameIs("binary"))
            ExtractImage(parser, tok);
        if (!inBody)
            continue;

        if (tok->IsText()) {
            htmlData.Append(tok->s, tok->sLen);
        }
        else if (tok->IsStartTag()) {
            if (Tag_P == tag && !inTitle) {
                htmlData.Append("<p>");
            }
            else if (Tag_Title == tag) {
                if (inTitle++)
                    /* ignore nested title tags */;
                else if (sectionDepth <= 5)
                    htmlData.AppendFmt("<h%d>", sectionDepth);
                else
                    htmlData.Append("<p><b>");
            }
            else if (Tag_Body == tag) {
                if (htmlData.Count() == 0)
                    htmlData.Append("<!doctype html><title></title>");
            }
            else if (Tag_Strong == tag) {
                htmlData.Append("<strong>");
            }
            else if (tok->NameIs("section")) {
                sectionDepth++;
            }
            else if (tok->NameIs("emphasis")) {
                htmlData.Append("<em>");
            }
            else if (tok->NameIs("epigraph")) {
                htmlData.Append("<blockquote>");
            }
            // TODO: handle Tag_A, <text-author>
        }
        else if (tok->IsEndTag()) {
            if (Tag_P == tag && !inTitle) {
                htmlData.Append("</p>");
            }
            else if (Tag_Title == tag && inTitle > 0) {
                if (--inTitle)
                    /* ignore nested title tags */;
                else if (sectionDepth <= 5)
                    htmlData.AppendFmt("</h%d>", sectionDepth);
                else
                    htmlData.Append("</b></p>");
            }
            else if (Tag_Strong == tag) {
                htmlData.Append("</strong>");
            }
            else if (tok->NameIs("section") && sectionDepth > 0) {
                sectionDepth--;
            }
            else if (tok->NameIs("emphasis")) {
                htmlData.Append("</em>");
            }
            else if (tok->NameIs("epigraph")) {
                htmlData.Append("</blockquote>");
            }
        }
        else if (tok->IsEmptyElementEndTag()) {
            if (tok->NameIs("image")) {
                AttrInfo *attrInfo = tok->GetAttrByName("xlink:href");
                if (attrInfo) {
                    ScopedMem<char> link(str::DupN(attrInfo->val, attrInfo->valLen));
                    htmlData.AppendFmt("<img src=\"%s\">", link);
                }
            }
            else if (tok->NameIs("empty-line")) {
                htmlData.Append("<p></p>");
            }
        }
    }

    return htmlData.Count() > 0;
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

char *Base64Decode(const char *s, const char *end, size_t *len)
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

void Fb2DocImpl::ExtractImage(HtmlPullParser& parser, HtmlToken *tok)
{
    ScopedMem<char> id;
    AttrInfo *attrInfo = tok->GetAttrByName("id");
    if (attrInfo)
        id.Set(str::DupN(attrInfo->val, attrInfo->valLen));

    tok = parser.Next();
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

Fb2Doc *Fb2Doc::ParseFile(const TCHAR *fileName)
{
    Fb2DocImpl *doc = new Fb2DocImpl(fileName);
    if (doc && !doc->Load()) {
        delete doc;
        doc = NULL;
    }
    return doc;
}

bool Fb2Doc::IsSupported(const TCHAR *fileName)
{
    return str::EndsWithI(fileName, _T(".fb2")) ||
           str::EndsWithI(fileName, _T(".fb2.zip"));
}
