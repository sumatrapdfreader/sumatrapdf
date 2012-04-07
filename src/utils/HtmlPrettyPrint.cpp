/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "HtmlPrettyPrint.h"
#include "HtmlPullParser.h"

static void HtmlAddWithNesting(str::Str<char>* out, HtmlToken *tok, HtmlTag tag, size_t nesting)
{
    CrashIf(!tok->IsStartTag() && !tok->IsEndTag() && !tok->IsEmptyElementEndTag());
    bool isInline = IsInlineTag(tag);
    // add a newline before block start tags (unless there already is one)
    bool onNewLine = out->Count() == 0 || out->Last() == '\n';
    if (!onNewLine && !isInline && !tok->IsEndTag()) {
        out->Append('\n');
        onNewLine = true;
    }
    // indent the tag if it starts on a new line
    if (onNewLine) {
        for (size_t i = 0; i < nesting; i++)
            out->Append('\t');
        if (tok->IsEndTag() && nesting > 0)
            out->Pop();
    }
    // output the tag and all its attributes
    out->Append('<');
    if (tok->IsEndTag())
        out->Append('/');
    // TODO: normalize whitespace between attributes?
    out->Append(tok->s, tok->sLen);
    if (tok->IsEmptyElementEndTag())
        out->Append('/');
    out->Append('>');
    // add a newline after block end tags
    if (!isInline && !tok->IsStartTag())
        out->Append('\n');
}

static bool IsWsText(const char *s, size_t len)
{
    const char *end = s + len;
    for (; s < end && str::IsWs(*s); s++);
    return s == end;
}

char *PrettyPrintHtml(const char *s, size_t len, size_t& lenOut)
{
    str::Str<char> res(len);
    HtmlPullParser parser(s, len);
    HtmlToken *t;
    size_t nesting = 0;
    while ((t = parser.Next()) && !t->IsError()) {
        if (t->IsText()) {
            // TODO: normalize whitespace instead?
            if (!IsWsText(t->s, t->sLen))
                res.Append(t->s, t->sLen);
        }
        if (!t->IsTag())
            continue;

        HtmlTag tag = FindTag(t);
        HtmlAddWithNesting(&res, t, tag, nesting);

        // determine the next tag's nesting before the
        // call to UpdateTagNesting so that the tag
        // itself is not yet in tagNesting
        nesting = parser.tagNesting.Count();
    }
    lenOut = res.Count();
    return res.StealData();
}
