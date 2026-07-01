/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"
#include "HtmlPrettyPrint.h"
#include "HtmlParserLookup.h"
#include "HtmlPullParser.h"

static void HtmlAddWithNesting(str::Builder* out, HtmlToken* tok, size_t nesting) {
    ReportIf(!tok->IsStartTag() && !tok->IsEndTag() && !tok->IsEmptyElementEndTag());
    bool isInline = IsInlineTag(tok->tag);
    // add a newline before block start tags (unless there already is one)
    bool onNewLine = len(*out) == 0 || out->Last() == '\n';
    if (!onNewLine && !isInline && !tok->IsEndTag()) {
        out->AppendChar('\n');
        onNewLine = true;
    }
    // indent the tag if it starts on a new line
    if (onNewLine) {
        for (size_t i = 0; i < nesting; i++) {
            out->AppendChar('\t');
        }
        if (tok->IsEndTag() && nesting > 0) {
            out->RemoveLast();
        }
    }
    // output the tag and all its attributes
    out->AppendChar('<');
    if (tok->IsEndTag()) {
        out->AppendChar('/');
    }
    // TODO: normalize whitespace between attributes?
    out->Append(tok->s);
    if (tok->IsEmptyElementEndTag()) {
        out->AppendChar('/');
    }
    out->AppendChar('>');
    // add a newline after block end tags
    if (!isInline && !tok->IsStartTag()) {
        out->AppendChar('\n');
    }
}

static bool IsWsText(Str s) {
    int off = 0;
    SkipWs(s, off);
    return off == s.len;
}

ByteSlice PrettyPrintHtml(const ByteSlice& d) {
    size_t n = d.size();
    str::Builder res(n);
    HtmlPullParser parser(d);
    Vec<HtmlTag> tagNesting;
    HtmlToken* t;
    while ((t = parser.Next()) != nullptr && !t->IsError()) {
        if (t->IsText()) {
            // TODO: normalize whitespace instead?
            if (!IsWsText(t->s)) {
                res.Append(t->s);
            }
        }
        if (!t->IsTag()) {
            continue;
        }

        HtmlAddWithNesting(&res, t, tagNesting.size());

        if (t->IsStartTag()) {
            if (!IsTagSelfClosing(t->tag)) {
                tagNesting.Append(t->tag);
            }
        } else if (t->IsEndTag()) {
            // when closing a tag, if the top tag doesn't match but
            // there are only potentially self-closing tags on the
            // stack between the matching tag, we pop all of them
            if (tagNesting.Contains(t->tag)) {
                while (tagNesting.Last() != t->tag) {
                    tagNesting.Pop();
                }
            }
            if (tagNesting.size() > 0 && tagNesting.Last() == t->tag) {
                tagNesting.Pop();
            }
        }
    }
    size_t sz = len(res);
    Str stolen = res.StealData();
    return {(u8*)stolen.s, sz};
}
