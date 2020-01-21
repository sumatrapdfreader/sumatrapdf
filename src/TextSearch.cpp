/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"

#define SkipWhitespace(c) for (; str::IsWs(*(c)); (c)++)
// ignore spaces between CJK glyphs but not between Latin, Greek, Cyrillic, etc. letters
// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=959
#define isnoncjkwordchar(c) (isWordChar(c) && (unsigned short)(c) < 0x2E80)

static void markAllPagesNonSkip(std::vector<bool>& pagesToSkip) {
    for (size_t i = 0; i < pagesToSkip.size(); i++) {
        pagesToSkip[i] = false;
    }
}
TextSearch::TextSearch(EngineBase* engine, PageTextCache* textCache) : TextSelection(engine, textCache) {
    nPages = engine->PageCount();
    pagesToSkip.resize(nPages);
    markAllPagesNonSkip(pagesToSkip);
}

TextSearch::~TextSearch() {
    Clear();
}

void TextSearch::Reset() {
    pageText = nullptr;
    TextSelection::Reset();
}

void TextSearch::SetText(const WCHAR* text) {
    // search text starting with a single space enables the 'Match word start'
    // and search text ending in a single space enables the 'Match word end' option
    // (that behavior already "kind of" exists without special treatment, but
    // usually is not quite what a user expects, so let's try to be cleverer)
    this->matchWordStart = text[0] == ' ' && text[1] != ' ';
    this->matchWordEnd = str::EndsWith(text, L" ") && !str::EndsWith(text, L"  ");

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
        const WCHAR* end;
        for (end = text; isnoncjkwordchar(*end); end++)
            ;
        anchor = str::DupN(text, end - text);
    }
    // Adobe Reader also matches certain hard-to-type Unicode
    // characters when searching for easy-to-type homoglyphs
    // cf. http://forums.fofou.org/sumatrapdf/topic?id=2432337
    else if (*text == '-' || *text == '\'' || *text == '"')
        anchor = nullptr;
    else
        anchor = str::DupN(text, 1);

    if (str::Len(this->findText) >= INT_MAX)
        this->findText[(unsigned)INT_MAX - 1] = '\0';
    if (str::EndsWith(this->findText, L" "))
        this->findText[str::Len(this->findText) - 1] = '\0';

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetSensitive(bool sensitive) {
    if (caseSensitive == sensitive) {
        return;
    }
    this->caseSensitive = sensitive;

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetDirection(TextSearchDirection direction) {
    bool forward = TextSearchDirection::Forward == direction;
    if (forward == this->forward)
        return;
    this->forward = forward;
    if (findText) {
        int n = (int)str::Len(findText);
        if (forward) {
            findIndex += n;
        } else {
            findIndex -= n;
        }
    }
}

void TextSearch::SetLastResult(TextSelection* sel) {
    CopySelection(sel);

    AutoFreeWstr selection(ExtractText(L" "));
    str::NormalizeWS(selection);
    SetText(selection);

    searchHitStartAt = findPage = std::min(startPage, endPage);
    findIndex = (findPage == startPage ? startGlyph : endGlyph) + (int)str::Len(findText);
    pageText = textCache->GetData(findPage);
    forward = true;
}

// try to match "findText" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
TextSearch::PageAndOffset TextSearch::MatchEnd(const WCHAR* start) const {
    const WCHAR *match = findText, *end = start;
    const PageAndOffset notFound = {-1, -1};
    int currentPage = findPage;
    const WCHAR* currentPageText = pageText;
    bool lookingAtWs;

    if (matchWordStart && start > pageText && isWordChar(start[-1]) && isWordChar(start[0]))
        return notFound;

    if (!match)
        return notFound;

    while (*match) {
        if (!*end)
            return notFound;
        /* Going from page n to page n+1 is a space, too.*/
        lookingAtWs = (!*end && (currentPage < nPages)) || str::IsWs(*end);
        if (caseSensitive ? *match == *end : CharLower((LPWSTR)LOWORD(*match)) == CharLower((LPWSTR)LOWORD(*end)))
            /* characters are identical */;
        else if (str::IsWs(*match) && lookingAtWs)
            /* treat all whitespace as identical and end of page as whitespace.
               The end of the document is NOT seen as whitespace */
            ;
        // TODO: Adobe Reader seems to have a more extensive list of
        //       normalizations - is there an easier way?
        else if (*match == '-' && (0x2010 <= *end && *end <= 0x2014))
            /* make HYPHEN-MINUS also match HYPHEN, NON-BREAKING HYPHEN,
               FIGURE DASH, EN DASH and EM DASH (but not the other way around) */
            ;
        else if (*match == '\'' && (0x2018 <= *end && *end <= 0x201b))
            /* make APOSTROPHE also match LEFT/RIGHT SINGLE QUOTATION MARK */;
        else if (*match == '"' && (0x201c <= *end && *end <= 0x201f))
            /* make QUOTATION MARK also match LEFT/RIGHT DOUBLE QUOTATION MARK */;
        else
            return notFound;
        match++;
        // We might get here either ...
        if (*end) {
            // ... because there's a genuine match -> consider next character in next loop iteration
            end++;
        } else {
            // ... or because we were looking at whitespace in the pattern and we were at a page break
            // -> skip to next page
            ++currentPage;
            end = currentPageText = textCache->GetData(currentPage);
        }
        // treat "??" and "? ?" differently, since '?' could have been a word
        // character that's just missing an encoding (and '?' is the replacement
        // character); cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1574
        if (*match && !isnoncjkwordchar(*(match - 1)) && (*(match - 1) != '?' || *match != '?') ||
            lookingAtWs && str::IsWs(*(match - 1))) {
            SkipWhitespace(match);
            SkipWhitespace(end);
            while ((!*end) && (currentPage < nPages)) {
                // treat page break as whitespace, too
                ++currentPage;
                end = currentPageText = textCache->GetData(currentPage);
                SkipWhitespace(end);
            }
        }
    }
    if (matchWordEnd && end > currentPageText && isWordChar(end[-1]) && isWordChar(end[0]))
        return notFound;

    int off = (int)(end - currentPageText);
    return {currentPage, off};
}

static const WCHAR* GetNextIndex(const WCHAR* base, int offset, bool forward) {
    const WCHAR* c = base + offset + (forward ? 0 : -1);
    if (c < base || !*c)
        return nullptr;
    return c;
}

bool TextSearch::FindTextInPage(int pageNo, TextSearch::PageAndOffset* finalGlyph) {
    if (str::IsEmpty(findText))
        return false;
    if (!pageNo)
        pageNo = findPage;
    // According to my analysis of 69912675c766b6325f38036913dcf0505a00be36, when we
    // get here with pageNo != 0 the findText has already been set so I didn't add
    // a findText = textCache->GetData(findPage) here.
    findPage = pageNo;

    const WCHAR* found;
    PageAndOffset fg;
    do {
        if (!anchor) {
            found = GetNextIndex(pageText, findIndex, forward);
        } else if (forward) {
            const WCHAR* s = pageText + findIndex;
            if (caseSensitive) {
                found = StrStr(s, anchor);
            } else {
                found = StrStrI(s, anchor);
            }
        } else {
            found = StrRStrI(pageText, pageText + findIndex, anchor);
        }
        if (!found)
            return false;
        findIndex = (int)(found - pageText) + (forward ? 1 : 0);
        fg = MatchEnd(found);
    } while (fg.page <= 0);

    int offset = (int)(found - pageText);
    searchHitStartAt = pageNo;
    StartAt(pageNo, offset);
    SelectUpTo(fg.page, fg.offset);
    findIndex = forward ? fg.offset : offset;

    // try again if the found text is completely outside the page's mediabox
    if (result.len == 0)
        return FindTextInPage(pageNo, finalGlyph);

    if (finalGlyph) {
        *finalGlyph = fg;
    }
    return true;
}

bool TextSearch::FindStartingAtPage(int pageNo, ProgressUpdateUI* tracker) {
    if (str::IsEmpty(findText))
        return false;

    int next = forward ? 1 : -1;
    while (1 <= pageNo && pageNo <= nPages && (!tracker || !tracker->WasCanceled())) {
        if (tracker) {
            tracker->UpdateProgress(pageNo, nPages);
        }

        if (pagesToSkip[pageNo - 1]) {
            pageNo += next;
            continue;
        }

        Reset();

        pageText = textCache->GetData(pageNo, &findIndex);
        if (pageText) {
            if (forward) {
                findIndex = 0;
            }
            PageAndOffset r;
            if (FindTextInPage(pageNo, &r)) {
                if (forward) {
                    if (findPage != r.page) {
                        findPage = r.page;
                        pageText = textCache->GetData(findPage);
                    }
                    findIndex = r.offset;
                }
                return true;
            }
            pagesToSkip[pageNo - 1] = true;
        }

        pageNo += next;
    }

    // allow for the first/last page to be included in the next search
    searchHitStartAt = findPage = forward ? nPages + 1 : 0;

    return false;
}

TextSel* TextSearch::FindFirst(int page, const WCHAR* text, ProgressUpdateUI* tracker) {
    SetText(text);

    if (FindStartingAtPage(page, tracker))
        return &result;
    return nullptr;
}

TextSel* TextSearch::FindNext(ProgressUpdateUI* tracker) {
    CrashIf(!findText);
    if (!findText)
        return nullptr;

    if (tracker) {
        if (tracker->WasCanceled()) {
            return nullptr;
        }
        tracker->UpdateProgress(findPage, nPages);
    }

    PageAndOffset finalGlyph;
    if (FindTextInPage(findPage, &finalGlyph)) {
        if (forward) {
            findPage = finalGlyph.page;
            findIndex = finalGlyph.offset;
            pageText = textCache->GetData(findPage);
        }
        return &result;
    }

    auto next = forward ? 1 : -1;
    if (FindStartingAtPage(findPage + next, tracker)) {
        return &result;
    }
    return nullptr;
}
