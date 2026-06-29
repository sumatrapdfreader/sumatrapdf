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

static void SkipWhitespaceIdx(WStr text, int& idx) {
    for (; idx < text.len && str::IsWs(text.s[idx]); idx++) {
    }
}
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
    wstr::FreePtr(&findText);
    wstr::FreePtr(&anchor);
    wstr::FreePtr(&lastText);
    Reset();
}

void TextSearch::Reset() {
    pageText = {};
    TextSelection::Reset();
}

int TextSearch::GetCurrentPageNo() const {
    return findPage;
}

// note: the result might not be a valid page number!
int TextSearch::GetSearchHitStartPageNo() const {
    return searchHitStartAt;
}

void TextSearch::SetText(WStr text) {
    // search text starting with a single space enables the 'Match word start'
    // and search text ending in a single space enables the 'Match word end' option
    // (that behavior already "kind of" exists without special treatment, but
    // usually is not quite what a user expects, so let's try to be cleverer)
    // "match whole word" forces both word-boundary checks on; otherwise they're
    // driven by a leading / trailing single space in the search text
    this->matchWordStart =
        matchWholeWord || (!wstr::IsEmpty(text) && text.s[0] == L' ' && (text.len < 2 || text.s[1] != L' '));
    this->matchWordEnd = matchWholeWord || (wstr::EndsWith(text, WStrL(L" ")) && !wstr::EndsWith(text, WStrL(L"  ")));

    WStr searchText = text;
    if (!wstr::IsEmpty(searchText) && searchText.s[0] == L' ') {
        searchText = WStr(searchText.s + 1, searchText.len - 1);
    }

    // don't reset anything if the search text hasn't changed at all
    if (wstr::Eq(this->lastText, searchText)) {
        return;
    }

    this->Clear();
    this->lastText = wstr::Dup(searchText);
    this->findText = wstr::Dup(searchText);

    // extract anchor string (the first word or the first symbol) for faster searching
    if (searchText && isnoncjkwordchar(searchText.s[0])) {
        int end = 0;
        for (; end < searchText.len && isnoncjkwordchar(searchText.s[end]); end++) {
            ;
        }
        anchor = wstr::Dup(WStr(searchText.s, (int)end));
    }
    // Adobe Reader also matches certain hard-to-type Unicode
    // characters when searching for easy-to-type homoglyphs
    // cf. https://web.archive.org/web/20140201013717/http://forums.fofou.org:80/sumatrapdf/topic?id=2432337&comments=3
    else if (searchText && (searchText.s[0] == L'-' || searchText.s[0] == L'\'' || searchText.s[0] == L'"')) {
        anchor = {};
    } else if (searchText) {
        anchor = wstr::Dup(WStr(searchText.s, 1));
    } else {
        anchor = {};
    }

    if (len(this->findText) >= INT_MAX) {
        this->findText.s[(unsigned)INT_MAX - 1] = '\0';
    }
    if (wstr::EndsWith(this->findText, WStrL(L" "))) {
        this->findText.s[len(this->findText) - 1] = '\0';
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
        int n = len(findText);
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
    wstr::NormalizeWSInPlace(WStr(selection.Get()));
    SetText(WStr(selection.Get()));

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
static bool MatchSearchUnit(WStr h, int hIdx, WStr n, int nIdx, int& hAdv, int& nAdv) {
    if (hIdx >= h.len || nIdx >= n.len) {
        return false;
    }
    WCHAR hc = h.s[hIdx];
    WCHAR nc = n.s[nIdx];
    // ß in the needle matches "ss" in the text
    if (IsSharpS(nc) && hIdx + 1 < h.len && IsLatinS(h.s[hIdx]) && IsLatinS(h.s[hIdx + 1])) {
        hAdv = 2;
        nAdv = 1;
        return true;
    }
    // "ss" in the needle matches ß in the text
    if (nIdx + 1 < n.len && IsLatinS(n.s[nIdx]) && IsLatinS(n.s[nIdx + 1]) && IsSharpS(hc)) {
        hAdv = 1;
        nAdv = 2;
        return true;
    }
    // everything else (including ß~ß and ss~ss) matches one-to-one
    if (FoldCaseForSearch(hc) == FoldCaseForSearch(nc)) {
        hAdv = 1;
        nAdv = 1;
        return true;
    }
    return false;
}

static WStr StrStrFoldCase(WStr haystack, WStr needle) {
    if (!haystack || !needle) {
        return haystack;
    }
    for (int i = 0; i < haystack.len && haystack.s[i]; i++) {
        int hIdx = i;
        int nIdx = 0;
        bool isMatch = true;
        while (nIdx < needle.len && needle.s[nIdx]) {
            if (hIdx >= haystack.len || !haystack.s[hIdx]) {
                isMatch = false;
                break;
            }
            int hAdv, nAdv;
            if (!MatchSearchUnit(haystack, hIdx, needle, nIdx, hAdv, nAdv)) {
                isMatch = false;
                break;
            }
            hIdx += hAdv;
            nIdx += nAdv;
        }
        if (isMatch) {
            return WStr(haystack.s + i, haystack.len - i);
        }
    }
    return {};
}

static WStr StrRStr(WStr text, int endOff, WStr needle) {
    if (!text || !needle || endOff <= 0 || endOff > text.len) {
        return {};
    }
    int needleLen = needle.len;
    if (needleLen <= 0 || needleLen > endOff) {
        return {};
    }
    for (int i = endOff - needleLen; i >= 0; i--) {
        if (memcmp(text.s + i, needle.s, (size_t)needleLen * sizeof(WCHAR)) == 0) {
            return WStr(text.s + i, endOff - i);
        }
    }
    return {};
}

static WStr StrRStrFoldCase(WStr text, int endOff, WStr needle) {
    if (!text || !needle || endOff <= 0 || endOff > text.len) {
        return {};
    }
    // ß <-> ss makes the matched length variable, so scan forward within
    // [start, end) and remember the last start position that matches.
    WStr result;
    for (int i = 0; i < endOff && text.s[i]; i++) {
        int hIdx = i;
        int nIdx = 0;
        bool isMatch = true;
        while (nIdx < needle.len && needle.s[nIdx]) {
            if (hIdx >= endOff || !text.s[hIdx]) {
                isMatch = false;
                break;
            }
            int hAdv, nAdv;
            if (!MatchSearchUnit(text, hIdx, needle, nIdx, hAdv, nAdv)) {
                isMatch = false;
                break;
            }
            hIdx += hAdv;
            nIdx += nAdv;
        }
        if (isMatch) {
            result = WStr(text.s + i, endOff - i);
        }
    }
    return result;
}

// try to match "findText" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
TextSearch::PageAndOffset TextSearch::MatchEnd(WStr start) const {
    const PageAndOffset notFound = {-1, -1};
    int currentPage = findPage;
    WStr currentPageText = pageText;
    bool lookingAtWs;

    int startOff = (int)(start.s - pageText.s);
    if (matchWordStart && startOff > 0 && isWordChar(pageText.s[startOff - 1]) && isWordChar(pageText.s[startOff])) {
        return notFound;
    }

    if (!findText) {
        return notFound;
    }

    int matchIdx = 0;
    int endIdx = startOff;
    while (matchIdx < findText.len && findText.s[matchIdx]) {
        bool atPageEnd = endIdx >= currentPageText.len || !currentPageText.s[endIdx];
        if (atPageEnd && currentPage >= nPages) {
            return notFound;
        }
        WCHAR endCh = atPageEnd ? 0 : currentPageText.s[endIdx];
        /* Going from page n to page n+1 is a space, too.*/
        lookingAtWs = (atPageEnd && (currentPage < nPages)) || str::IsWs(endCh);
        bool isMatch = false;
        // extra advance for the German ß <-> ss equivalence, where one side
        // consumes one WCHAR and the other two (issue #933)
        int extraMatchAdv = 0;
        int extraEndAdv = 0;
        WCHAR matchCh = findText.s[matchIdx];
        if (matchCase) {
            isMatch = matchCh == endCh;
        } else {
            isMatch = FoldCaseForSearch(matchCh) == FoldCaseForSearch(endCh);
            if (!isMatch) {
                if (IsSharpS(matchCh) && !atPageEnd && endIdx + 1 < currentPageText.len && IsLatinS(endCh) &&
                    IsLatinS(currentPageText.s[endIdx + 1])) {
                    // ß in the search text matches "ss" in the page
                    isMatch = true;
                    extraEndAdv = 1;
                } else if (matchIdx + 1 < findText.len && IsLatinS(findText.s[matchIdx]) &&
                           IsLatinS(findText.s[matchIdx + 1]) && IsSharpS(endCh)) {
                    // "ss" in the search text matches ß in the page
                    isMatch = true;
                    extraMatchAdv = 1;
                }
            }
        }
        if (isMatch) {
            /* characters are identical */;
        } else if (str::IsWs(matchCh) && lookingAtWs) {
            /* treat all whitespace as identical and end of page as whitespace.
               The end of the document is NOT seen as whitespace */
            ;
            // TODO: Adobe Reader seems to have a more extensive list of
            //       normalizations - is there an easier way?
        } else if (matchCh == L'-' && (0x2010 <= endCh && endCh <= 0x2014)) {
            /* make HYPHEN-MINUS also match HYPHEN, NON-BREAKING HYPHEN,
               FIGURE DASH, EN DASH and EM DASH (but not the other way around) */
            ;
        } else if (matchCh == L'\'' && (0x2018 <= endCh && endCh <= 0x201b)) {
            /* make APOSTROPHE also match LEFT/RIGHT SINGLE QUOTATION MARK */;
        } else if (matchCh == L'"' && (0x201c <= endCh && endCh <= 0x201f)) {
            /* make QUOTATION MARK also match LEFT/RIGHT DOUBLE QUOTATION MARK */;
        } else {
            return notFound;
        }
        // consume the extra char on whichever side of a ß <-> ss match is longer
        matchIdx += extraMatchAdv;
        endIdx += extraEndAdv;
        matchIdx++;
        // We might get here either ...
        if (!atPageEnd && endCh) {
            // ... because there's a genuine match -> consider next character in next loop iteration
            endIdx++;
        } else {
            // ... or because we were looking at whitespace in the pattern and we were at a page break
            // -> skip to next page
            ++currentPage;
            currentPageText = engine->GetTextForPage(currentPage);
            endIdx = 0;
        }
        // treat "??" and "? ?" differently, since '?' could have been a word
        // character that's just missing an encoding (and '?' is the replacement
        // character); cf. https://code.google.com/archive/p/sumatrapdf/issues/1574
        if (matchIdx < findText.len && findText.s[matchIdx] &&
            ((!isnoncjkwordchar(findText.s[matchIdx - 1]) &&
              (findText.s[matchIdx - 1] != L'?' || findText.s[matchIdx] != L'?')) ||
             (lookingAtWs && str::IsWs(findText.s[matchIdx - 1])))) {
            SkipWhitespaceIdx(findText, matchIdx);
            SkipWhitespaceIdx(currentPageText, endIdx);
            while (endIdx >= currentPageText.len && currentPage < nPages) {
                // treat page break as whitespace, too
                ++currentPage;
                currentPageText = engine->GetTextForPage(currentPage);
                endIdx = 0;
                SkipWhitespaceIdx(currentPageText, endIdx);
            }
        }
    }
    if (matchWordEnd && endIdx > 0 && endIdx < currentPageText.len && isWordChar(currentPageText.s[endIdx - 1]) &&
        isWordChar(currentPageText.s[endIdx])) {
        return notFound;
    }

    return {currentPage, endIdx};
}

static WStr WStrStr(WStr haystack, WStr needle) {
    if (!haystack || wstr::IsEmpty(needle)) {
        return {};
    }
    for (int i = 0; i <= haystack.len - needle.len; i++) {
        if (memcmp(haystack.s + i, needle.s, (size_t)needle.len * sizeof(WCHAR)) == 0) {
            return WStr(haystack.s + i, haystack.len - i);
        }
    }
    return {};
}

static WStr GetNextIndex(WStr base, int offset, bool forward) {
    int idx = offset + (forward ? 0 : -1);
    if (idx < 0 || idx >= base.len || !base.s[idx]) {
        return {};
    }
    return WStr(base.s + idx, base.len - idx);
}

bool TextSearch::FindTextInPage(int pageNo, TextSearch::PageAndOffset* finalGlyph) {
    if (wstr::IsEmpty(findText)) {
        return false;
    }
    if (!pageNo) {
        pageNo = findPage;
    }
    // According to my analysis of 69912675c766b6325f38036913dcf0505a00be36, when we
    // get here with pageNo != 0 the findText has already been set so I didn't add
    // a findText = engine->GetTextForPage(findPage) here.
    findPage = pageNo;

    WStr found;
    PageAndOffset fg;
    do {
        if (!anchor) {
            found = GetNextIndex(pageText, findIndex, forward);
        } else if (forward) {
            WStr s(pageText.s + findIndex, pageText.len - findIndex);
            if (matchCase) {
                found = WStrStr(s, anchor);
            } else {
                found = StrStrFoldCase(s, anchor);
            }
        } else {
            if (matchCase) {
                found = StrRStr(pageText, findIndex, anchor);
            } else {
                found = StrRStrFoldCase(pageText, findIndex, anchor);
            }
        }
        if (!found) {
            return false;
        }
        findIndex = (int)(found.s - pageText.s) + (forward ? 1 : 0);
        fg = MatchEnd(found);
    } while (fg.page <= 0);

    int offset = (int)(found.s - pageText.s);
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
    if (wstr::IsEmpty(findText)) {
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

TextSel* TextSearch::FindFirst(int page, WStr text) {
    SetText(text);

    if (FindStartingAtPage(page)) {
        return &result;
    }
    return nullptr;
}

// search only `pageNo` (no wrapping to other pages), mirroring the per-page step
// inside FindStartingAtPage. Used for page-constrained search (issue #3085)
TextSel* TextSearch::FindFirstOnPage(int pageNo, WStr text) {
    SetText(text);
    if (wstr::IsEmpty(findText) || pageNo < 1 || pageNo > nPages) {
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
