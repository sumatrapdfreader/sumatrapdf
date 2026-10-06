/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "DocController.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"

// Fetch page text for search. When *abortSearch is set, the caller should stop
// immediately (search was cancelled while engine locks were contended).
static Str GetSearchPageText(EngineBase* engine, int pageNo, int* lenOut, const ProgressUpdateCb& progressCb,
                             bool* abortSearch) {
    if (abortSearch) {
        *abortSearch = false;
    }
    if (engine->TryGetTextForPage(pageNo, lenOut) || !WasCanceled(progressCb)) {
        return engine->GetTextForPage(pageNo, lenOut);
    }
    if (abortSearch) {
        *abortSearch = true;
    }
    if (lenOut) {
        *lenOut = 0;
    }
    return {};
}

static void SkipWhitespace(Str text, int textLen, int& idx, int& byteIdx) {
    while (idx < textLen) {
        int nextByte = byteIdx;
        int c = Utf8CodepointNext(text, nextByte);
        if (!str::IsWs((char)c)) {
            break;
        }
        byteIdx = nextByte;
        idx++;
    }
}
// ignore spaces between CJK glyphs but not between Latin, Greek, Cyrillic, etc. letters
// cf. https://code.google.com/archive/p/sumatrapdf/issues/959
#define isnoncjkwordchar(c) (isWordChar(c) && (unsigned short)(c) < 0x2E80)

static void markAllPagesNonSkip(Vec<bool>& pagesToSkip) {
    for (int i = 0; i < len(pagesToSkip); i++) {
        pagesToSkip[i] = false;
    }
}
TextSearch::TextSearch(EngineBase* engine) : TextSelection(engine) {
    nPages = engine->PageCount();
    VecResize(pagesToSkip, nPages);
}

TextSearch::~TextSearch() {
    Clear();
}

void TextSearch::Clear() {
    findText = {};
    anchor = {};
    str::FreePtr(&lastText);
    findTextLen = 0;
    anchorLen = 0;
    Reset();
}

void TextSearch::Reset() {
    pageText = {};
    pageTextLen = 0;
    TextSelection::Reset();
}

int TextSearch::GetCurrentPageNo() const {
    return findPage;
}

// note: the result might not be a valid page number!
int TextSearch::GetSearchHitStartPageNo() const {
    return searchHitStartAt;
}

void TextSearch::SetText(Str text) {
    // Single leading/trailing spaces request word boundaries; whole-word mode
    // requests both. Strip one space from each end for matching.
    this->matchWordStart = matchWholeWord || (text && text.s[0] == ' ' && (text.len < 2 || text.s[1] != ' '));
    this->matchWordEnd = matchWholeWord || (str::EndsWith(text, StrL(" ")) && !str::EndsWith(text, StrL("  ")));

    Str searchText = text;
    if (searchText && searchText.s[0] == ' ') {
        searchText = Str(searchText.s + 1, searchText.len - 1);
    }

    // don't reset anything if the search text hasn't changed at all
    if (str::Eq(this->lastText, searchText)) {
        return;
    }

    this->Clear();
    // Matching text and anchor borrow the saved query.
    this->lastText = str::Dup(searchText);
    searchText = this->lastText;
    this->findText = searchText;
    this->findTextLen = Utf8CodepointCount(this->findText);

    // extract anchor string (the first word or the first symbol) for faster searching
    int searchTextLen = findTextLen;
    int firstCharEndByte = 0;
    int firstChar = Utf8CodepointNext(searchText, firstCharEndByte);
    if (searchTextLen > 0 && isnoncjkwordchar(firstChar)) {
        int end = 1;
        int endByte = firstCharEndByte;
        while (end < searchTextLen) {
            int nextByte = endByte;
            int c = Utf8CodepointNext(searchText, nextByte);
            if (!isnoncjkwordchar(c)) {
                break;
            }
            endByte = nextByte;
            end++;
        }
        anchor = Str(searchText.s, endByte);
        anchorLen = end;
    }
    // Homoglyphs need the full matcher, so do not use them as anchors.
    else if (searchTextLen > 0 && firstChar != '-' && firstChar != '\'' && firstChar != '"') {
        anchor = Str(searchText.s, firstCharEndByte);
        anchorLen = 1;
    }

    if (str::EndsWith(this->findText, StrL(" "))) {
        this->findText.len--;
        this->findTextLen--;
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

bool TextSearch::PageAllowed(int pageNo) const {
    if (pageNo < 1 || pageNo > nPages) {
        return false;
    }
    if (len(pageAllowed) == 0) {
        return true;
    }
    if (pageNo > len(pageAllowed)) {
        return false;
    }
    return pageAllowed[pageNo - 1];
}

int TextSearch::RestrictFirst() const {
    if (len(pageAllowed) == 0) {
        return 1;
    }
    int n = std::min(len(pageAllowed), nPages);
    for (int i = 0; i < n; i++) {
        if (pageAllowed[i]) {
            return i + 1;
        }
    }
    return 1;
}

int TextSearch::RestrictLast() const {
    if (len(pageAllowed) == 0) {
        return nPages;
    }
    int last = 0;
    int n = std::min(len(pageAllowed), nPages);
    for (int i = 0; i < n; i++) {
        if (pageAllowed[i]) {
            last = i + 1;
        }
    }
    return last > 0 ? last : nPages;
}

void TextSearch::SetAllowedPages(const Vec<bool>& allowed) {
    pageAllowed = allowed;
    markAllPagesNonSkip(pagesToSkip);
}

void TextSearch::SetDirection(TextSearch::Direction direction) {
    bool fwd = TextSearch::Direction::Forward == direction;
    if (fwd == forward) {
        return;
    }
    forward = fwd;
    if (findText) {
        int n = findTextLen;
        if (fwd) {
            findIndex += n;
        } else {
            findIndex -= n;
        }
    }
}

void TextSearch::SetLastResult(TextSelection* sel) {
    CopySelection(sel);

    Str selection = ExtractText(StrL(" "));
    selection.len -= str::NormalizeWSInPlace(selection);
    SetText(selection);
    str::Free(selection);

    searchHitStartAt = findPage = std::min(startPage, endPage);
    findPage = std::max(startPage, endPage);
    findIndex = (findPage == endPage ? endGlyph : startGlyph);
    pageText = engine->GetTextForPage(findPage, &pageTextLen);
    forward = true;
}

// case-insensitive search also ignores diacritics: "lacz" finds "Łącz"
static int FoldCaseForSearch(int c) {
    return FoldDiacriticsRune(FoldCaseRune(c));
}

// German ß (sharp s, U+00DF) is spelled "ss" and the two are often used
// interchangeably, so for case-insensitive search we treat ß as equivalent to
// "ss" (issue #933). Fold first so capital ẞ (U+1E9E) and case differences work.
static bool IsSharpS(int c) {
    return c != 0 && FoldCaseForSearch(c) == 0x00DF;
}
static bool IsLatinS(int c) {
    return c != 0 && FoldCaseForSearch(c) == L's';
}

// Match one folded unit, treating ß and "ss" as equivalent.
// Advances report codepoints and bytes consumed on each side.
static bool MatchSearchUnit(Str h, int hLen, int hIdx, int hByteIdx, Str n, int nLen, int nIdx, int nByteIdx, int& hAdv,
                            int& nAdv, int& hByteAdv, int& nByteAdv) {
    hAdv = nAdv = hByteAdv = nByteAdv = 0;
    if (hIdx >= hLen || nIdx >= nLen) {
        return false;
    }
    int hNextByte = hByteIdx;
    int hc = Utf8CodepointNext(h, hNextByte);
    int nNextByte = nByteIdx;
    int nc = Utf8CodepointNext(n, nNextByte);
    // ß in the needle matches "ss" in the text
    if (IsSharpS(nc) && hIdx + 1 < hLen && IsLatinS(hc)) {
        int hAfterNextByte = hNextByte;
        int hNextChar = Utf8CodepointNext(h, hAfterNextByte);
        if (IsLatinS(hNextChar)) {
            hAdv = 2;
            nAdv = 1;
            hByteAdv = hAfterNextByte - hByteIdx;
            nByteAdv = nNextByte - nByteIdx;
            return true;
        }
    }
    // "ss" in the needle matches ß in the text
    if (nIdx + 1 < nLen && IsLatinS(nc) && IsSharpS(hc)) {
        int nAfterNextByte = nNextByte;
        int nNextChar = Utf8CodepointNext(n, nAfterNextByte);
        if (IsLatinS(nNextChar)) {
            hAdv = 1;
            nAdv = 2;
            hByteAdv = hNextByte - hByteIdx;
            nByteAdv = nAfterNextByte - nByteIdx;
            return true;
        }
    }
    // everything else (including ß~ß and ss~ss) matches one-to-one
    if (FoldCaseForSearch(hc) == FoldCaseForSearch(nc)) {
        hAdv = 1;
        nAdv = 1;
        hByteAdv = hNextByte - hByteIdx;
        nByteAdv = nNextByte - nByteIdx;
        return true;
    }
    return false;
}

static bool MatchesFoldedAt(Str text, int textLen, int idx, int byteIdx, Str needle, int needleLen, int limit) {
    int nIdx = 0;
    int nByteIdx = 0;
    while (nIdx < needleLen) {
        if (idx >= limit) {
            return false;
        }
        int hAdv, nAdv, hByteAdv, nByteAdv;
        if (!MatchSearchUnit(text, textLen, idx, byteIdx, needle, needleLen, nIdx, nByteIdx, hAdv, nAdv, hByteAdv,
                             nByteAdv)) {
            return false;
        }
        idx += hAdv;
        nIdx += nAdv;
        byteIdx += hByteAdv;
        nByteIdx += nByteAdv;
    }
    return true;
}

static int FindFirstFolded(Str haystack, int haystackLen, int startOff, Str needle, int needleLen) {
    // nothing to find in an empty page: reporting a hit made the caller retry forever
    if (len(haystack) == 0) {
        return -1;
    }
    if (len(needle) == 0) {
        return startOff;
    }
    int byteIdx = Utf8CodepointToByteIndex(haystack, startOff);
    for (int i = startOff; i < haystackLen; i++) {
        if (MatchesFoldedAt(haystack, haystackLen, i, byteIdx, needle, needleLen, haystackLen)) {
            return i;
        }
        Utf8CodepointNext(haystack, byteIdx);
    }
    return -1;
}

static bool StartsWithAtByte(Str text, int byteIdx, Str prefix) {
    return text && prefix && byteIdx >= 0 && byteIdx + prefix.len <= text.len &&
           memcmp(text.s + byteIdx, prefix.s, prefix.len) == 0;
}

static int FindLastExact(Str text, int textLen, int endOff, Str needle, int needleLen) {
    if (len(text) == 0 || len(needle) == 0 || endOff <= 0 || endOff > textLen) {
        return -1;
    }
    if (needleLen <= 0 || needleLen > endOff) {
        return -1;
    }
    int result = -1;
    int byteIdx = 0;
    for (int i = 0; i <= endOff - needleLen; i++) {
        if (StartsWithAtByte(text, byteIdx, needle)) {
            result = i;
        }
        Utf8CodepointNext(text, byteIdx);
    }
    return result;
}

static int FindLastFolded(Str text, int textLen, int endOff, Str needle, int needleLen) {
    if (len(text) == 0 || len(needle) == 0 || endOff <= 0 || endOff > textLen) {
        return -1;
    }
    // ß <-> ss makes the matched length variable, so scan forward within
    // [start, end) and remember the last start position that matches.
    int result = -1;
    int byteIdx = 0;
    for (int i = 0; i < endOff; i++) {
        if (MatchesFoldedAt(text, textLen, i, byteIdx, needle, needleLen, endOff)) {
            result = i;
        }
        Utf8CodepointNext(text, byteIdx);
    }
    return result;
}

// try to match "findText" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
TextSearch::PageAndOffset TextSearch::MatchEnd(int startOff) const {
    const PageAndOffset notFound = {-1, -1};
    int currentPage = findPage;
    Str currentPageText = pageText;
    int currentPageTextLen = pageTextLen;
    bool lookingAtWs;

    if (len(findText) == 0) {
        return notFound;
    }

    int matchIdx = 0;
    int matchByteIdx = 0;
    int endIdx = startOff;
    int endByteIdx = Utf8CodepointToByteIndex(currentPageText, endIdx);

    if (matchWordStart && startOff > 0) {
        int prevByteIdx = endByteIdx;
        int prevCh = Utf8CodepointPrev(pageText, prevByteIdx);
        int nextByteIdx = endByteIdx;
        int curCh = Utf8CodepointNext(pageText, nextByteIdx);
        if (isWordChar(prevCh) && isWordChar(curCh)) {
            return notFound;
        }
    }

    while (matchIdx < findTextLen) {
        bool atPageEnd = endIdx >= currentPageTextLen;
        if (atPageEnd && currentPage >= nPages) {
            return notFound;
        }
        int endNextByteIdx = endByteIdx;
        int endCh = atPageEnd ? 0 : Utf8CodepointNext(currentPageText, endNextByteIdx);
        /* Going from page n to page n+1 is a space, too.*/
        lookingAtWs = (atPageEnd && (currentPage < nPages)) || str::IsWs((char)endCh);
        bool isMatch = false;
        // extra advance for the German ß <-> ss equivalence, where one side
        // consumes one codepoint and the other two (issue #933)
        int extraMatchAdv = 0;
        int extraEndAdv = 0;
        int matchNextByteIdx = matchByteIdx;
        int matchCh = Utf8CodepointNext(findText, matchNextByteIdx);
        if (matchCase) {
            isMatch = matchCh == endCh;
        } else {
            isMatch = FoldCaseForSearch(matchCh) == FoldCaseForSearch(endCh);
            if (!isMatch && !atPageEnd) {
                int hAdv, nAdv, hByteAdv, nByteAdv;
                isMatch = MatchSearchUnit(currentPageText, currentPageTextLen, endIdx, endByteIdx, findText,
                                          findTextLen, matchIdx, matchByteIdx, hAdv, nAdv, hByteAdv, nByteAdv);
                if (isMatch) {
                    extraEndAdv = hAdv - 1;
                    extraMatchAdv = nAdv - 1;
                    endNextByteIdx = endByteIdx + hByteAdv;
                    matchNextByteIdx = matchByteIdx + nByteAdv;
                }
            }
        }
        bool sameWhitespace = str::IsWs((char)matchCh) && lookingAtWs;
        // ASCII punctuation also matches typographic variants, in this direction only.
        bool samePunctuation = (matchCh == '-' && 0x2010 <= endCh && endCh <= 0x2014) ||
                               (matchCh == '\'' && 0x2018 <= endCh && endCh <= 0x201b) ||
                               (matchCh == '"' && 0x201c <= endCh && endCh <= 0x201f);
        if (!isMatch && !sameWhitespace && !samePunctuation) {
            return notFound;
        }
        // consume the extra char on whichever side of a ß <-> ss match is longer
        int matchAdv = 1 + extraMatchAdv;
        matchByteIdx = matchNextByteIdx;
        matchIdx += matchAdv;
        // We might get here either ...
        if (!atPageEnd && endCh) {
            // ... because there's a genuine match -> consider next character in next loop iteration
            int endAdv = 1 + extraEndAdv;
            endByteIdx = endNextByteIdx;
            endIdx += endAdv;
        } else {
            // ... or because we were looking at whitespace in the pattern and we were at a page break
            // -> skip to next page (but not past a restricted range)
            ++currentPage;
            if (!PageAllowed(currentPage)) {
                return notFound;
            }
            bool abortSearch = false;
            currentPageText = GetSearchPageText(engine, currentPage, &currentPageTextLen, progressCb, &abortSearch);
            if (abortSearch) {
                return notFound;
            }
            endIdx = 0;
            endByteIdx = 0;
        }
        // treat "??" and "? ?" differently, since '?' could have been a word
        // character that's just missing an encoding (and '?' is the replacement
        // character); cf. https://code.google.com/archive/p/sumatrapdf/issues/1574
        int prevMatchByteIdx = matchByteIdx;
        int prevMatchCh = Utf8CodepointPrev(findText, prevMatchByteIdx);
        int curMatchCh = Utf8CodepointAtByte(findText, matchByteIdx);
        if (matchIdx < findTextLen && ((!isnoncjkwordchar(prevMatchCh) && (prevMatchCh != '?' || curMatchCh != '?')) ||
                                       (lookingAtWs && str::IsWs((char)prevMatchCh)))) {
            SkipWhitespace(findText, findTextLen, matchIdx, matchByteIdx);
            SkipWhitespace(currentPageText, currentPageTextLen, endIdx, endByteIdx);
            while (endIdx >= currentPageTextLen && PageAllowed(currentPage + 1)) {
                // treat page break as whitespace, too
                ++currentPage;
                bool abortSearch = false;
                currentPageText = GetSearchPageText(engine, currentPage, &currentPageTextLen, progressCb, &abortSearch);
                if (abortSearch) {
                    return notFound;
                }
                endIdx = 0;
                endByteIdx = 0;
                SkipWhitespace(currentPageText, currentPageTextLen, endIdx, endByteIdx);
            }
        }
    }
    if (matchWordEnd && endIdx > 0 && endIdx < currentPageTextLen) {
        int prevByteIdx = endByteIdx;
        int prevCh = Utf8CodepointPrev(currentPageText, prevByteIdx);
        int nextByteIdx = endByteIdx;
        int curCh = Utf8CodepointNext(currentPageText, nextByteIdx);
        if (isWordChar(prevCh) && isWordChar(curCh)) {
            return notFound;
        }
    }

    return {currentPage, endIdx};
}

static int FindFirstExact(Str haystack, int haystackLen, int startOff, Str needle, int needleLen) {
    if (len(haystack) == 0 || len(needle) == 0) {
        return -1;
    }
    int byteIdx = Utf8CodepointToByteIndex(haystack, startOff);
    for (int i = startOff; i <= haystackLen - needleLen; i++) {
        if (StartsWithAtByte(haystack, byteIdx, needle)) {
            return i;
        }
        Utf8CodepointNext(haystack, byteIdx);
    }
    return -1;
}

static int GetNextIndex(int textLen, int offset, bool forward) {
    int idx = offset + (forward ? 0 : -1);
    if (idx < 0 || idx >= textLen) {
        return -1;
    }
    return idx;
}

bool TextSearch::FindTextInPage(int pageNo, TextSearch::PageAndOffset* finalGlyph) {
    if (len(findText) == 0) {
        return false;
    }
    if (!pageNo) {
        pageNo = findPage;
    }
    findPage = pageNo;

    int found = -1;
    PageAndOffset fg;
    for (;;) {
        do {
            if (WasCanceled(progressCb)) {
                return false;
            }
            if (len(anchor) == 0) {
                found = GetNextIndex(pageTextLen, findIndex, forward);
            } else {
                auto find = forward ? (matchCase ? FindFirstExact : FindFirstFolded)
                                    : (matchCase ? FindLastExact : FindLastFolded);
                found = find(pageText, pageTextLen, findIndex, anchor, anchorLen);
            }
            if (found < 0) {
                return false;
            }
            findIndex = found + (forward ? 1 : 0);
            fg = MatchEnd(found);
        } while (fg.page <= 0);

        int offset = found;
        searchHitStartAt = pageNo;
        StartAt(pageNo, offset);
        SelectUpTo(fg.page, fg.offset);
        findIndex = forward ? fg.offset : offset;

        // try again if the found text is completely outside the page's mediabox
        if (len(result) != 0) {
            break;
        }
    }

    if (finalGlyph) {
        *finalGlyph = fg;
    }
    return true;
}

// a chaptered doc may have laid out only the first chapter when this
// TextSearch was constructed; lay out the rest so a whole-document find
// covers every page, and grow pagesToSkip to match
void TextSearch::EnsureFullyLaidOut() {
    EnsureFullLayout(engine);
    int newPages = engine->PageCount();
    if (newPages == nPages) {
        return;
    }
    int oldPages = nPages;
    nPages = newPages;
    VecResize(pagesToSkip, nPages);
    for (int i = oldPages; i < nPages; i++) {
        pagesToSkip[i] = false;
    }
}

bool TextSearch::FindStartingAtPage(int pageNo) {
    if (len(findText) == 0) {
        return false;
    }

    EnsureFullyLaidOut();

    int lo = RestrictFirst();
    int hi = RestrictLast();
    if (pageNo < lo) {
        pageNo = forward ? lo : 0;
    } else if (pageNo > hi) {
        pageNo = forward ? nPages + 1 : hi;
    }

    int next = forward ? 1 : -1;
    while ((lo <= pageNo) && (pageNo <= hi) && !WasCanceled(progressCb)) {
        UpdateProgress(progressCb, pageNo, nPages);

        if (!PageAllowed(pageNo) || pagesToSkip[pageNo - 1]) {
            pageNo += next;
            continue;
        }

        PageSearchResult found = SearchPage(pageNo);
        if (found == PageSearchResult::Canceled) {
            break;
        }
        if (found != PageSearchResult::Found) {
            if (found == PageSearchResult::NotFound) {
                pagesToSkip[pageNo - 1] = true;
            }
            pageNo += next;
            continue;
        }
        return true;
    }

    // allow for the first/last page of the (restricted) range to be included next
    searchHitStartAt = findPage = forward ? hi + 1 : lo - 1;

    return false;
}

Vec<TextSel>* TextSearch::FindFirst(int page, Str text) {
    SetText(text);

    if (FindStartingAtPage(page)) {
        return &result;
    }
    return nullptr;
}

TextSearch::PageSearchResult TextSearch::SearchPage(int pageNo) {
    Reset();
    bool abortSearch = false;
    pageText = GetSearchPageText(engine, pageNo, &pageTextLen, progressCb, &abortSearch);
    if (abortSearch) {
        return PageSearchResult::Canceled;
    }
    findIndex = pageTextLen;
    if (len(pageText) == 0) {
        return PageSearchResult::Empty;
    }
    if (forward) {
        findIndex = 0;
    }
    PageAndOffset r;
    if (!FindTextInPage(pageNo, &r)) {
        return PageSearchResult::NotFound;
    }
    if (forward) {
        if (findPage != r.page) {
            findPage = r.page;
            pageText = GetSearchPageText(engine, findPage, &pageTextLen, progressCb, &abortSearch);
            if (abortSearch) {
                return PageSearchResult::Canceled;
            }
        }
        findIndex = r.offset;
    }
    return PageSearchResult::Found;
}

Vec<TextSel>* TextSearch::FindFirstOnPage(int pageNo, Str text) {
    SetText(text);
    if (len(findText) == 0 || pageNo < 1 || pageNo > nPages) {
        return nullptr;
    }
    return SearchPage(pageNo) == PageSearchResult::Found ? &result : nullptr;
}

Vec<TextSel>* TextSearch::FindNext() {
    ReportIf(len(findText) == 0);
    if (len(findText) == 0) {
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
            bool abortSearch = false;
            pageText = GetSearchPageText(engine, findPage, &pageTextLen, progressCb, &abortSearch);
            if (abortSearch) {
                return nullptr;
            }
        }
        return &result;
    }

    auto next = forward ? 1 : -1;
    if (FindStartingAtPage(findPage + next)) {
        return &result;
    }
    return nullptr;
}
