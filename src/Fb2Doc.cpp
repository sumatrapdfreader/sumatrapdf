/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Fb2Doc.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "HtmlPullParser.h"

/*
Basic FB2 format support:
- convert basic FB2 markup into HTML

TODO:
- convert links, styles and images to HTML
- extract metadata
- extract basic document styles from stylesheet
*/

Fb2Doc::Fb2Doc(const TCHAR *fileName) :
    fileName(str::Dup(fileName)) {
}

char *Fb2Doc::GetBookHtmlData(size_t& lenOut)
{
    lenOut = htmlData.Size();
    return htmlData.Get();
}

inline bool IsTag(HtmlToken *tok, const char *tag)
{
    return str::EqN(tok->s, tag, tok->sLen);
}

bool Fb2Doc::Load()
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
    Fb2Doc *doc = new Fb2Doc(fileName);
    if (doc && !doc->Load()) {
        delete doc;
        doc = NULL;
    }
    return doc;
}
