/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "TextSearch.h"
#include "StrUtil.h"
#include <shlwapi.h>

enum { SEARCH_PAGE, SKIP_PAGE };

#define SkipWhitespace(c) for (; _istspace(*(c)); (c)++)
// ignore spaces between CJK glyphs but not between Latin, Greek, Cyrillic, etc. letters
// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=959
#define isnoncjkwordchar(c) (iswordchar(c) && (unsigned short)(c) < 0x2E80)

TextSearch::TextSearch(BaseEngine *engine) : TextSelection(engine),
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
int TextSearch::MatchLen(TCHAR *start)
{
    TCHAR *match = findText, *end = start;
    assert(!_istspace(*end));

    if (matchWordStart && start > pageText && iswordchar(start[-1]) && iswordchar(start[0]))
        return -1;

    while (*match) {
        if (!*end)
            return -1;
        if (_istspace(*match) && _istspace(*end))
            /* treat all whitespace as identical */;
        else if (caseSensitive ? *match != *end : CharLower((LPTSTR)LOWORD(*match)) != CharLower((LPTSTR)LOWORD(*end)))
            return -1;
        match++;
        end++;
        if (!isnoncjkwordchar(*(match - 1)) || _istspace(*(match - 1)) && _istspace(*(end - 1))) {
            SkipWhitespace(match);
            SkipWhitespace(end);
        }
    }

    if (matchWordEnd && end > pageText && iswordchar(end[-1]) && iswordchar(end[0]))
        return -1;

    return (int)(end - start);
}

bool TextSearch::FindTextInPage(int pageNo)
{
    if (str::IsEmpty(anchor))
        return false;
    if (!pageNo)
        pageNo = findPage;
    findPage = pageNo;

    TCHAR *found;
    int length;
    do {
        if (forward)
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
    if (str::IsEmpty(anchor))
        return false;

    int total = engine->PageCount();
    while (1 <= pageNo && pageNo <= total &&
           (!tracker || tracker->UpdateProgress(pageNo, total))) {
        if (SKIP_PAGE == findCache[pageNo - 1]) {
            pageNo += forward ? 1 : -1;
            continue;
        }

        Reset();

        // make sure that the page text has been cached
        if (!text[pageNo - 1])
            FindClosestGlyph(pageNo, 0, 0);

        pageText = text[pageNo - 1];
        findIndex = forward ? 0 : lens[pageNo - 1];

        if (pageText) {
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
    assert(findText);
    if (!findText)
        return NULL;
    if (tracker && !tracker->UpdateProgress(findPage, engine->PageCount()))
        return NULL;

    if (FindTextInPage())
        return &result;
    if (FindStartingAtPage(findPage + (forward ? 1 : -1), tracker))
        return &result;
    return NULL;
}
