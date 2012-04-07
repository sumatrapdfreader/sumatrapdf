/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "TextSearch.h"

enum { SEARCH_PAGE, SKIP_PAGE };

#define SkipWhitespace(c) for (; _istspace(*(c)); (c)++)
// ignore spaces between CJK glyphs but not between Latin, Greek, Cyrillic, etc. letters
// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=959
#define isnoncjkwordchar(c) (iswordchar(c) && (unsigned short)(c) < 0x2E80)

TextSearch::TextSearch(BaseEngine *engine, PageTextCache *textCache) :
    TextSelection(engine, textCache),
    findText(NULL), anchor(NULL), pageText(NULL),
    caseSensitive(false), forward(true),
    matchWordStart(false), matchWordEnd(false),
    findPage(0), findIndex(0), lastText(NULL)
{
    findCache = SAZA(BYTE, this->engine->PageCount());
}

TextSearch::~TextSearch()
{
    Clear();
    free(findCache);
}

void TextSearch::Reset()
{
    pageText = NULL;
    TextSelection::Reset();
}

void TextSearch::SetText(TCHAR *text)
{
    // search text starting with a single space enables the 'Match word start'
    // and search text ending in a single space enables the 'Match word end' option
    // (that behavior already "kind of" exists without special treatment, but
    // usually is not quite what a user expects, so let's try to be cleverer)
    this->matchWordStart = text[0] == ' ' && text[1] != ' ';
    this->matchWordEnd = str::EndsWith(text, _T(" ")) && !str::EndsWith(text, _T("  "));

    if (text[0] == ' ')
        text++;

    // don't reset anything if the search text hasn't changed at all
    if (str::Eq(this->lastText, text))
        return;

    this->Clear();
    this->lastText = str::Dup(text);
    this->findText = str::Dup(text);

    // extract anchor string (the first word or the first symbol) for faster searching
    if (isnoncjkwordchar(*text)) {
        TCHAR *end;
        for (end = text; isnoncjkwordchar(*end); end++)
            ;
        anchor = str::DupN(text, end - text);
    }
#ifdef UNICODE
    // Adobe Reader also matches certain hard-to-type Unicode
    // characters when searching for easy-to-type homoglyphs
    // cf. http://forums.fofou.org/sumatrapdf/topic?id=2432337
    else if (*text == '-' || *text == '\'' || *text == '"')
        anchor = NULL;
#endif
    else
        anchor = str::DupN(text, 1);

    if (str::EndsWith(this->findText, _T(" ")))
        this->findText[str::Len(this->findText) - 1] = '\0';

    memset(this->findCache, SEARCH_PAGE, this->engine->PageCount());
}

void TextSearch::SetSensitive(bool sensitive)
{
    if (caseSensitive == sensitive)
        return;
    this->caseSensitive = sensitive;

    memset(this->findCache, SEARCH_PAGE, this->engine->PageCount());
}

void TextSearch::SetDirection(TextSearchDirection direction)
{
    bool forward = FIND_FORWARD == direction;
    if (forward == this->forward)
        return;
    this->forward = forward;
    if (findText)
        findIndex += (int)str::Len(findText) * (forward ? 1 : -1);
}

// try to match "findText" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
int TextSearch::MatchLen(const TCHAR *start)
{
    const TCHAR *match = findText, *end = start;

    if (matchWordStart && start > pageText && iswordchar(start[-1]) && iswordchar(start[0]))
        return -1;

    if (!match)
        return -1;

    while (*match) {
        if (!*end)
            return -1;
        if (caseSensitive ? *match != *end : CharLower((LPTSTR)LOWORD(*match)) == CharLower((LPTSTR)LOWORD(*end)))
            /* characters are identical */;
        else if (_istspace(*match) && _istspace(*end))
            /* treat all whitespace as identical */;
#ifdef UNICODE
        // TODO: Adobe Reader seems to have a more extensive list of
        //       normalizations - is there an easier way?
        else if (*match == '-' && (0x2010 <= *end && *end <= 0x2014))
            /* make HYPHEN-MINUS also match HYPHEN, NON-BREAKING HYPHEN,
               FIGURE DASH, EN DASH and EM DASH (but not the other way around) */;
        else if (*match == '\'' && (0x2018 <= *end && *end <= 0x201b))
            /* make APOSTROPHE also match LEFT/RIGHT SINGLE QUOTATION MARK */;
        else if (*match == '"' && (0x201c <= *end && *end <= 0x201f))
            /* make QUOTATION MARK also match LEFT/RIGHT DOUBLE QUOTATION MARK */;
#endif
        else
            return -1;
        match++;
        end++;
        // treat "??" and "? ?" differently, since '?' could have been a word
        // character that's just missing an encoding (and '?' is the replacement
        // character); cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1574
        if (*match && !isnoncjkwordchar(*(match - 1)) && (*(match - 1) != '?' || *match != '?') ||
            _istspace(*(match - 1)) && _istspace(*(end - 1))) {
            SkipWhitespace(match);
            SkipWhitespace(end);
        }
    }

    if (matchWordEnd && end > pageText && iswordchar(end[-1]) && iswordchar(end[0]))
        return -1;

    return (int)(end - start);
}

static const TCHAR *GetNextIndex(const TCHAR *base, int offset, bool forward)
{
    const TCHAR *c = base + offset + (forward ? 0 : -1);
    if (c < base || !*c)
        return NULL;
    return c;
}

bool TextSearch::FindTextInPage(int pageNo)
{
    if (str::IsEmpty(findText))
        return false;
    if (!pageNo)
        pageNo = findPage;
    findPage = pageNo;

    const TCHAR *found;
    int length;
    do {
        if (!anchor)
            found = GetNextIndex(pageText, findIndex, forward);
        else if (forward)
            found = (caseSensitive ? StrStr : StrStrI)(pageText + findIndex, anchor);
        else
            found = StrRStrI(pageText, pageText + findIndex, anchor);
        if (!found)
            return false;
        findIndex = (int)(found - pageText) + (forward ? 1 : 0);
        length = MatchLen(found);
    } while (length <= 0);

    int offset = (int)(found - pageText);
    StartAt(pageNo, offset);
    SelectUpTo(pageNo, offset + length);
    findIndex = offset + (forward ? length : 0);

    // try again if the found text is completely outside the page's mediabox
    if (result.len == 0)
        return FindTextInPage(pageNo);

    return true;
}

bool TextSearch::FindStartingAtPage(int pageNo, ProgressUpdateUI *tracker)
{
    if (str::IsEmpty(findText))
        return false;

    int total = engine->PageCount();
    while (1 <= pageNo && pageNo <= total && (!tracker || !tracker->WasCanceled())) {
        if (tracker)
            tracker->UpdateProgress(pageNo, total);

        if (SKIP_PAGE == findCache[pageNo - 1]) {
            pageNo += forward ? 1 : -1;
            continue;
        }

        Reset();

        pageText = textCache->GetData(pageNo, &findIndex);
        if (pageText) {
            if (forward)
                findIndex = 0;
            if (FindTextInPage(pageNo))
                return true;
            findCache[pageNo - 1] = SKIP_PAGE;
        }

        pageNo += forward ? 1 : -1;
    }

    // allow for the first/last page to be included in the next search
    findPage = forward ? total + 1 : 0;

    return false;
}

TextSel *TextSearch::FindFirst(int page, TCHAR *text, ProgressUpdateUI *tracker)
{
    SetText(text);

    if (FindStartingAtPage(page, tracker))
        return &result;
    return NULL;
}

TextSel *TextSearch::FindNext(ProgressUpdateUI *tracker)
{
    CrashIf(!findText);
    if (!findText)
        return NULL;

    if (tracker) {
        if (tracker->WasCanceled())
            return NULL;
        tracker->UpdateProgress(findPage, engine->PageCount());
    }

    if (FindTextInPage())
        return &result;
    if (FindStartingAtPage(findPage + (forward ? 1 : -1), tracker))
        return &result;
    return NULL;
}
