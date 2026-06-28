/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"

#define SkipWhitespace(c) for (; str::IsWs(*(c)); (c)++)
// ignore spaces between CJK glyphs but not between Latin, Greek, Cyrillic, etc. letters
// cf. https://code.google.com/archive/p/sumatrapdf/issues/959
#define isnoncjkwordchar(c) (isWordChar(c) && (unsigned short)(c) < 0x2E80)

static void markAllPagesNonSkip(Vec<bool>& pagesToSkip) {
    for (size_t i = 0; i < pagesToSkip.size(); i++) {
        pagesToSkip[i] = false;
    }
}
TextSearch::TextSearch(EngineBase* engine) : TextSelection(engine) {
    nPages = engine->PageCount();
    pagesToSkip.SetSize(nPages);
    markAllPagesNonSkip(pagesToSkip);
}

TextSearch::~TextSearch() {
    Clear();
}

void TextSearch::Clear() {
    str::FreePtr(&findText);
    str::FreePtr(&anchor);
    str::FreePtr(&lastText);
    Reset();
}

void TextSearch::Reset() {
    pageText = nullptr;
    TextSelection::Reset();
}

int TextSearch::GetCurrentPageNo() const {
    return findPage;
}

// note: the result might not be a valid page number!
int TextSearch::GetSearchHitStartPageNo() const {
    return searchHitStartAt;
}

void TextSearch::SetText(const WCHAR* text) {
    // search text starting with a single space enables the 'Match word start'
    // and search text ending in a single space enables the 'Match word end' option
    // (that behavior already "kind of" exists without special treatment, but
    // usually is not quite what a user expects, so let's try to be cleverer)
    // "match whole word" forces both word-boundary checks on; otherwise they're
    // driven by a leading / trailing single space in the search text
    this->matchWordStart = matchWholeWord || (text[0] == ' ' && text[1] != ' ');
    this->matchWordEnd = matchWholeWord || (str::EndsWith(text, L" ") && !str::EndsWith(text, L"  "));

    if (text[0] == ' ') {
        text++;
    }

    // don't reset anything if the search text hasn't changed at all
    if (str::Eq(this->lastText, text)) {
        return;
    }

    this->Clear();
    this->lastText = str::Dup(text);
    this->findText = str::Dup(text);

    // extract anchor string (the first word or the first symbol) for faster searching
    if (isnoncjkwordchar(*text)) {
        const WCHAR* end;
        for (end = text; isnoncjkwordchar(*end); end++) {
            ;
        }
        anchor = str::Dup(text, end - text);
    }
    // Adobe Reader also matches certain hard-to-type Unicode
    // characters when searching for easy-to-type homoglyphs
    // cf. https://web.archive.org/web/20140201013717/http://forums.fofou.org:80/sumatrapdf/topic?id=2432337&comments=3
    else if (*text == '-' || *text == '\'' || *text == '"') {
        anchor = nullptr;
    } else {
        anchor = str::Dup(text, 1);
    }

    if (str::Len(this->findText) >= INT_MAX) {
        this->findText[(unsigned)INT_MAX - 1] = '\0';
    }
    if (str::EndsWith(this->findText, L" ")) {
        this->findText[str::Len(this->findText) - 1] = '\0';
    }

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetMatchCase(bool newMatchCase) {
    if (matchCase == newMatchCase) {
        return;
    }
    this->matchCase = newMatchCase;

    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetMatchWholeWord(bool newMatchWholeWord) {
    if (matchWholeWord == newMatchWholeWord) {
        return;
    }
    this->matchWholeWord = newMatchWholeWord;
    // matchWordStart/matchWordEnd are recomputed from matchWholeWord on the next
    // SetText() (the re-search after a toggle always calls it), so we only need
    // to invalidate the per-page skip cache here, like SetMatchCase().
    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetDirection(TextSearch::Direction direction) {
    bool fwd = TextSearch::Direction::Forward == direction;
    if (fwd == forward) {
        return;
    }
    forward = fwd;
    if (findText) {
        int n = (int)str::Len(findText);
        if (fwd) {
            findIndex += n;
        } else {
            findIndex -= n;
        }
    }
}

void TextSearch::SetLastResult(TextSelection* sel) {
    CopySelection(sel);

    AutoFreeWStr selection(ExtractText(" ").s);
    str::NormalizeWSInPlace(selection);
    SetText(selection);

    searchHitStartAt = findPage = std::min(startPage, endPage);
    findPage = std::max(startPage, endPage);
    findIndex = (findPage == endPage ? endGlyph : startGlyph);
    pageText = engine->GetTextForPage(findPage);
    forward = true;
}

// Locale-independent Unicode case folding for search. CharLowerW folds accented
// letters (e.g. É->é, Ş->ş) regardless of the CRT locale, unlike towlower() or
// the ASCII-only fast paths we used before.
static WCHAR FoldCaseForSearch(WCHAR c) {
    // U+0130 (İ, Latin capital I with dot above) lowercases to 'i' under
    // standard Unicode case folding, but CharLowerW only does this under a
    // Turkish system locale and otherwise leaves it unchanged -- so searching
    // "ibradı" wouldn't find "İbradı" on non-Turkish systems (issue #5597).
    // Fold it explicitly so search is case-insensitive regardless of locale.
    if (c == 0x0130) {
        return L'i';
    }
    return (WCHAR)(uintptr_t)CharLowerW((LPWSTR)(uintptr_t)c);
}

// German ß (sharp s, U+00DF) is spelled "ss" and the two are often used
// interchangeably, so for case-insensitive search we treat ß as equivalent to
// "ss" (issue #933). Fold first so capital ẞ (U+1E9E) and case differences work.
static bool IsSharpS(WCHAR c) {
    return c != 0 && FoldCaseForSearch(c) == 0x00DF;
}
static bool IsLatinS(WCHAR c) {
    return c != 0 && FoldCaseForSearch(c) == L's';
}

// Compare needle `n` against haystack `h` for a single search "unit", case-
// folded, treating ß as equivalent to "ss". On a match returns true and reports
// how many WCHARs were consumed from each side (1:1 normally, but 1:2 / 2:1 for
// the ß <-> ss equivalence). Safe to call at a string end (reads at most h[1]
// / n[1], which is the NUL terminator at worst).
static bool MatchSearchUnit(const WCHAR* h, const WCHAR* n, int& hAdv, int& nAdv) {
    // ß in the needle matches "ss" in the text
    if (IsSharpS(*n) && IsLatinS(h[0]) && IsLatinS(h[1])) {
        hAdv = 2;
        nAdv = 1;
        return true;
    }
    // "ss" in the needle matches ß in the text
    if (IsLatinS(n[0]) && IsLatinS(n[1]) && IsSharpS(*h)) {
        hAdv = 1;
        nAdv = 2;
        return true;
    }
    // everything else (including ß~ß and ss~ss) matches one-to-one
    if (FoldCaseForSearch(*h) == FoldCaseForSearch(*n)) {
        hAdv = 1;
        nAdv = 1;
        return true;
    }
    return false;
}

static const WCHAR* StrStrFoldCase(const WCHAR* haystack, const WCHAR* needle) {
    if (!haystack || !needle || !*needle) {
        return haystack;
    }
    for (const WCHAR* s = haystack; *s; s++) {
        const WCHAR* h = s;
        const WCHAR* n = needle;
        bool isMatch = true;
        while (*n) {
            if (!*h) {
                isMatch = false;
                break;
            }
            int hAdv, nAdv;
            if (!MatchSearchUnit(h, n, hAdv, nAdv)) {
                isMatch = false;
                break;
            }
            h += hAdv;
            n += nAdv;
        }
        if (isMatch) {
            return s;
        }
    }
    return nullptr;
}

static const WCHAR* StrRStr(const WCHAR* start, const WCHAR* end, const WCHAR* needle) {
    if (!start || !end || !needle || !*needle || start >= end) {
        return nullptr;
    }
    size_t needleLen = str::Len(needle);
    if (needleLen > (size_t)(end - start)) {
        return nullptr;
    }
    const WCHAR* s = end - needleLen;
    for (; s >= start; s--) {
        if (memcmp(s, needle, needleLen * sizeof(WCHAR)) == 0) {
            return s;
        }
    }
    return nullptr;
}

static const WCHAR* StrRStrFoldCase(const WCHAR* start, const WCHAR* end, const WCHAR* needle) {
    if (!start || !end || !needle || !*needle || start >= end) {
        return nullptr;
    }
    // ß <-> ss makes the matched length variable, so scan forward within
    // [start, end) and remember the last start position that matches.
    const WCHAR* result = nullptr;
    for (const WCHAR* s = start; s < end && *s; s++) {
        const WCHAR* h = s;
        const WCHAR* n = needle;
        bool isMatch = true;
        while (*n) {
            if (h >= end || !*h) {
                isMatch = false;
                break;
            }
            int hAdv, nAdv;
            if (!MatchSearchUnit(h, n, hAdv, nAdv)) {
                isMatch = false;
                break;
            }
            h += hAdv;
            n += nAdv;
        }
        if (isMatch) {
            result = s;
        }
    }
    return result;
}

// try to match "findText" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
TextSearch::PageAndOffset TextSearch::MatchEnd(const WCHAR* start) const {
    const WCHAR *match = findText, *end = start;
    const PageAndOffset notFound = {-1, -1};
    int currentPage = findPage;
    const WCHAR* currentPageText = pageText;
    bool lookingAtWs;

    if (matchWordStart && start > pageText && isWordChar(start[-1]) && isWordChar(start[0])) {
        return notFound;
    }

    if (!match) {
        return notFound;
    }

    while (*match) {
        if (!*end) {
            return notFound;
        }
        /* Going from page n to page n+1 is a space, too.*/
        lookingAtWs = (!*end && (currentPage < nPages)) || str::IsWs(*end);
        bool isMatch = false;
        // extra advance for the German ß <-> ss equivalence, where one side
        // consumes one WCHAR and the other two (issue #933)
        int extraMatchAdv = 0;
        int extraEndAdv = 0;
        if (matchCase) {
            isMatch = *match == *end;
        } else {
            isMatch = FoldCaseForSearch(*match) == FoldCaseForSearch(*end);
            if (!isMatch) {
                if (IsSharpS(*match) && IsLatinS(end[0]) && IsLatinS(end[1])) {
                    // ß in the search text matches "ss" in the page
                    isMatch = true;
                    extraEndAdv = 1;
                } else if (IsLatinS(match[0]) && IsLatinS(match[1]) && IsSharpS(*end)) {
                    // "ss" in the search text matches ß in the page
                    isMatch = true;
                    extraMatchAdv = 1;
                }
            }
        }
        if (isMatch) {
            /* characters are identical */;
        } else if (str::IsWs(*match) && lookingAtWs) {
            /* treat all whitespace as identical and end of page as whitespace.
               The end of the document is NOT seen as whitespace */
            ;
            // TODO: Adobe Reader seems to have a more extensive list of
            //       normalizations - is there an easier way?
        } else if (*match == '-' && (0x2010 <= *end && *end <= 0x2014)) {
            /* make HYPHEN-MINUS also match HYPHEN, NON-BREAKING HYPHEN,
               FIGURE DASH, EN DASH and EM DASH (but not the other way around) */
            ;
        } else if (*match == '\'' && (0x2018 <= *end && *end <= 0x201b)) {
            /* make APOSTROPHE also match LEFT/RIGHT SINGLE QUOTATION MARK */;
        } else if (*match == '"' && (0x201c <= *end && *end <= 0x201f)) {
            /* make QUOTATION MARK also match LEFT/RIGHT DOUBLE QUOTATION MARK */;
        } else {
            return notFound;
        }
        // consume the extra char on whichever side of a ß <-> ss match is longer
        match += extraMatchAdv;
        end += extraEndAdv;
        match++;
        // We might get here either ...
        if (*end) {
            // ... because there's a genuine match -> consider next character in next loop iteration
            end++;
        } else {
            // ... or because we were looking at whitespace in the pattern and we were at a page break
            // -> skip to next page
            ++currentPage;
            end = currentPageText = engine->GetTextForPage(currentPage);
        }
        // treat "??" and "? ?" differently, since '?' could have been a word
        // character that's just missing an encoding (and '?' is the replacement
        // character); cf. https://code.google.com/archive/p/sumatrapdf/issues/1574
        if (*match && !isnoncjkwordchar(*(match - 1)) && (*(match - 1) != '?' || *match != '?') ||
            lookingAtWs && str::IsWs(*(match - 1))) {
            SkipWhitespace(match);
            SkipWhitespace(end);
            while ((!*end) && (currentPage < nPages)) {
                // treat page break as whitespace, too
                ++currentPage;
                end = currentPageText = engine->GetTextForPage(currentPage);
                SkipWhitespace(end);
            }
        }
    }
    if (matchWordEnd && end > currentPageText && isWordChar(end[-1]) && isWordChar(end[0])) {
        return notFound;
    }

    int off = (int)(end - currentPageText);
    return {currentPage, off};
}

static const WCHAR* GetNextIndex(const WCHAR* base, int offset, bool forward) {
    const WCHAR* c = base + offset + (forward ? 0 : -1);
    if (c < base || !*c) {
        return nullptr;
    }
    return c;
}

bool TextSearch::FindTextInPage(int pageNo, TextSearch::PageAndOffset* finalGlyph) {
    if (str::IsEmpty(findText)) {
        return false;
    }
    if (!pageNo) {
        pageNo = findPage;
    }
    // According to my analysis of 69912675c766b6325f38036913dcf0505a00be36, when we
    // get here with pageNo != 0 the findText has already been set so I didn't add
    // a findText = engine->GetTextForPage(findPage) here.
    findPage = pageNo;

    const WCHAR* found;
    PageAndOffset fg;
    do {
        if (!anchor) {
            found = GetNextIndex(pageText, findIndex, forward);
        } else if (forward) {
            const WCHAR* s = pageText + findIndex;
            if (matchCase) {
                found = StrStr(s, anchor);
            } else {
                found = StrStrFoldCase(s, anchor);
            }
        } else {
            if (matchCase) {
                found = StrRStr(pageText, pageText + findIndex, anchor);
            } else {
                found = StrRStrFoldCase(pageText, pageText + findIndex, anchor);
            }
        }
        if (!found) {
            return false;
        }
        findIndex = (int)(found - pageText) + (forward ? 1 : 0);
        fg = MatchEnd(found);
    } while (fg.page <= 0);

    int offset = (int)(found - pageText);
    searchHitStartAt = pageNo;
    StartAt(pageNo, offset);
    SelectUpTo(fg.page, fg.offset);
    findIndex = forward ? fg.offset : offset;

    // try again if the found text is completely outside the page's mediabox
    if (result.len == 0) {
        return FindTextInPage(pageNo, finalGlyph);
    }

    if (finalGlyph) {
        *finalGlyph = fg;
    }
    return true;
}

bool TextSearch::FindStartingAtPage(int pageNo) {
    if (str::IsEmpty(findText)) {
        return false;
    }

    int next = forward ? 1 : -1;
    while ((1 <= pageNo) && (pageNo <= nPages) && !WasCanceled(progressCb)) {
        UpdateProgress(progressCb, pageNo, nPages);

        if (pagesToSkip[pageNo - 1]) {
            pageNo += next;
            continue;
        }

        Reset();

        pageText = engine->GetTextForPage(pageNo, &findIndex);
        if (pageText) {
            if (forward) {
                findIndex = 0;
            }
            PageAndOffset r;
            if (FindTextInPage(pageNo, &r)) {
                if (forward) {
                    if (findPage != r.page) {
                        findPage = r.page;
                        pageText = engine->GetTextForPage(findPage);
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

TextSel* TextSearch::FindFirst(int page, const WCHAR* text) {
    SetText(text);

    if (FindStartingAtPage(page)) {
        return &result;
    }
    return nullptr;
}

// search only `pageNo` (no wrapping to other pages), mirroring the per-page step
// inside FindStartingAtPage. Used for page-constrained search (issue #3085)
TextSel* TextSearch::FindFirstOnPage(int pageNo, const WCHAR* text) {
    SetText(text);
    if (str::IsEmpty(findText) || pageNo < 1 || pageNo > nPages) {
        return nullptr;
    }
    Reset();
    pageText = engine->GetTextForPage(pageNo, &findIndex);
    if (!pageText) {
        return nullptr;
    }
    if (forward) {
        findIndex = 0;
    }
    PageAndOffset r;
    if (!FindTextInPage(pageNo, &r)) {
        return nullptr;
    }
    if (forward) {
        if (findPage != r.page) {
            findPage = r.page;
            pageText = engine->GetTextForPage(findPage);
        }
        findIndex = r.offset;
    }
    return &result;
}

TextSel* TextSearch::FindNext() {
    ReportIf(!findText);
    if (!findText) {
        return nullptr;
    }

    if (WasCanceled(progressCb)) {
        return nullptr;
    }
    UpdateProgress(progressCb, findPage, nPages);

    PageAndOffset finalGlyph;
    if (FindTextInPage(findPage, &finalGlyph)) {
        if (forward) {
            findPage = finalGlyph.page;
            findIndex = finalGlyph.offset;
            pageText = engine->GetTextForPage(findPage);
        }
        return &result;
    }

    auto next = forward ? 1 : -1;
    if (FindStartingAtPage(findPage + next)) {
        return &result;
    }
    return nullptr;
}
