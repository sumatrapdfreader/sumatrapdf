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

class CFb2Doc : public Fb2Doc {
    friend Fb2Doc;

    ScopedMem<TCHAR> fileName;
    str::Str<char> htmlData;

    bool Load();

public:
    CFb2Doc(const TCHAR *fileName) : fileName(str::Dup(fileName)) { }

    virtual const char *GetBookHtmlData(size_t& lenOut) {
        lenOut = htmlData.Size();
        return htmlData.Get();
    }
};

inline bool IsFb2Tag(HtmlToken *tok, const char *tag)
{
    size_t len = str::Len(tag);
    return GetTagLen(tok->s, tok->sLen) == len &&
           str::EqN(tok->s, tag, len);
}

inline bool IsFb2Attr(AttrInfo *attrInfo, const char *name)
{
    size_t len = str::Len(name);
    return len == attrInfo->nameLen && str::EqN(attrInfo->name, name, len);
}

bool CFb2Doc::Load()
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
        HtmlTag tag = tok->IsTag() ? FindTag(tok->s, tok->sLen) : Tag_NotFound;
        if (!inBody && tok->IsStartTag() && Tag_Body == tag)
            inBody++;
        else if (inBody && tok->IsEndTag() && Tag_Body == tag)
            inBody--;
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
            else if (IsFb2Tag(tok, "section")) {
                sectionDepth++;
            }
            else if (IsFb2Tag(tok, "emphasis")) {
                htmlData.Append("<em>");
            }
            else if (IsFb2Tag(tok, "epigraph")) {
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
            else if (IsFb2Tag(tok, "section") && sectionDepth > 0) {
                sectionDepth--;
            }
            else if (IsFb2Tag(tok, "emphasis")) {
                htmlData.Append("</em>");
            }
            else if (IsFb2Tag(tok, "epigraph")) {
                htmlData.Append("</blockquote>");
            }
        }
        else if (tok->IsEmptyElementEndTag()) {
            if (IsFb2Tag(tok, "image")) {
                AttrInfo *attrInfo;
                while ((attrInfo = tok->NextAttr())) {
                    if (IsFb2Attr(attrInfo, "xlink:href")) {
                        ScopedMem<char> link(str::DupN(attrInfo->val, attrInfo->valLen));
                    }
                }
            }
            else if (IsFb2Tag(tok, "empty-line")) {
                htmlData.Append("<p></p>");
            }
        }
    }

    return htmlData.Count() > 0;
}

Fb2Doc *Fb2Doc::ParseFile(const TCHAR *fileName)
{
    CFb2Doc *doc = new CFb2Doc(fileName);
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
