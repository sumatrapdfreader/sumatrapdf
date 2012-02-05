/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

#include "BaseUtil.h"
#include "Vec.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "HtmlPullParser.h"

#include "Fb2Doc.h"

/*
Basic FB2 format support:
- convert basic FB2 markup into HTML

TODO:
- convert links, styles and images to HTML
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

inline bool IsTag(HtmlToken *tok, const char *tag)
{
    return str::EqN(tok->s, tag, tok->sLen);
}

bool CFb2Doc::Load()
{
    size_t len;
    ScopedMem<char> data(file::ReadAll(fileName, &len));
    if (!data)
        return false;

    HtmlPullParser parser(data, len);
    HtmlToken *tok;
    int inBody = 0, inTitle = 0;
    int sectionDepth = 1;
    while ((tok = parser.Next())) {
        if (!inBody && tok->IsStartTag() && IsTag(tok, "body"))
            inBody++;
        else if (inBody && tok->IsEndTag() && IsTag(tok, "body"))
            inBody--;
        if (!inBody)
            continue;

        if (tok->IsText()) {
            htmlData.Append(tok->s, tok->sLen);
        }
        else if (tok->IsStartTag()) {
            if (IsTag(tok, "section")) {
                sectionDepth++;
            }
            else if (IsTag(tok, "p") && !inTitle) {
                htmlData.Append("<p>");
            }
            else if (IsTag(tok, "title")) {
                if (!inTitle++) {
                    if (sectionDepth <= 5)
                        htmlData.AppendFmt("<h%d>", sectionDepth);
                    else
                        htmlData.Append("<p><b>");
                }
            }
            else if (IsTag(tok, "body")) {
                if (htmlData.Count() == 0)
                    htmlData.Append("<!doctype html><title></title>");
            }
        }
        else if (tok->IsEndTag()) {
            if (IsTag(tok, "section") && sectionDepth > 0) {
                sectionDepth--;
            }
            else if (IsTag(tok, "p") && !inTitle) {
                htmlData.Append("</p>");
            }
            else if (IsTag(tok, "title") && inTitle > 0) {
                if (!--inTitle) {
                    if (sectionDepth <= 5)
                        htmlData.AppendFmt("</h%d>", sectionDepth);
                    else
                        htmlData.Append("</b></p>");
                }
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
    return str::EndsWithI(fileName, _T(".fb2"));
}
