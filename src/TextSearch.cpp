/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "DocController.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"

// Fetch page text for search. When *abortSearch is set, the caller should stop
// immediately (search was cancelled while engine locks were contended).
static Str GetTextForPageForSearch(EngineBase* engine, int pageNo, int* lenOut, const ProgressUpdateCb& progressCb,
                                   bool* abortSearch) {
    if (abortSearch) {
        *abortSearch = false;
    }
    if (!engine->TryGetTextForPage(pageNo, lenOut)) {
        if (WasCanceled(progressCb)) {
            if (abortSearch) {
                *abortSearch = true;
            }
            if (lenOut) {
                *lenOut = 0;
            }
            return {};
        }
        return engine->GetTextForPage(pageNo, lenOut);
    }
    return engine->GetTextForPage(pageNo, lenOut);
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
    // search text starting with a single space enables the 'Match word start'
    // and search text ending in a single space enables the 'Match word end' option
    // (that behavior already "kind of" exists without special treatment, but
    // usually is not quite what a user expects, so let's try to be cleverer)
    // "match whole word" forces both word-boundary checks on; otherwise they're
    // driven by a leading / trailing single space in the search text
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
    this->lastText = str::Dup(searchText);
    this->findText = str::Dup(searchText);
    this->findTextLen = Utf8CodepointCount(this->findText);

    // extract anchor string (the first word or the first symbol) for faster searching
    int searchTextLen = Utf8CodepointCount(searchText);
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
        anchor = str::Dup(Str(searchText.s, endByte));
        anchorLen = end;
    }
    // Adobe Reader also matches certain hard-to-type Unicode
    // characters when searching for easy-to-type homoglyphs
    // cf. https://web.archive.org/web/20140201013717/http://forums.fofou.org:80/sumatrapdf/topic?id=2432337&comments=3
    else if (searchTextLen > 0 && (firstChar == '-' || firstChar == '\'' || firstChar == '"')) {
        anchor = {};
    } else if (searchTextLen > 0) {
        anchor = str::Dup(Str(searchText.s, firstCharEndByte));
        anchorLen = 1;
    } else {
        anchor = {};
    }

    if (str::EndsWith(this->findText, StrL(" "))) {
        this->findText.s[len(this->findText) - 1] = '\0';
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

    Str selection = ExtractText(" ");
    selection.len -= str::NormalizeWSInPlace(selection);
    SetText(selection);
    str::Free(selection);

    searchHitStartAt = findPage = std::min(startPage, endPage);
    findPage = std::max(startPage, endPage);
    findIndex = (findPage == endPage ? endGlyph : startGlyph);
    pageText = engine->GetTextForPage(findPage, &pageTextLen);
    forward = true;
}

#if !OS_WIN
static int FoldCaseWCharPortable(int c) {
    if (c >= L'A' && c <= L'Z') {
        return c + 32;
    }
    if (c >= 0x00C0 && c <= 0x00DE && c != 0x00D7) {
        return c + 32;
    }
    if (c >= 0x0410 && c <= 0x042F) {
        return c + 32;
    }
    if (c == 0x0401) {
        return 0x0451;
    }
    if ((c >= 0x0391 && c <= 0x03A1) || (c >= 0x03A3 && c <= 0x03AB)) {
        return c + 32;
    }
    return (int)towlower((wint_t)c);
}
#endif

// Locale-independent Unicode case folding for search. CharLowerW folds accented
// letters (e.g. É->é, Ş->ş) regardless of the CRT locale, unlike towlower() or
// the ASCII-only fast paths we used before.
static int FoldCaseForSearch(int c) {
    // U+0130 (İ, Latin capital I with dot above) lowercases to 'i' under
    // standard Unicode case folding, but CharLowerW only does this under a
    // Turkish system locale and otherwise leaves it unchanged -- so searching
    // "ibradı" wouldn't find "İbradı" on non-Turkish systems (issue #5597).
    // Fold it explicitly so search is case-insensitive regardless of locale.
    if (c == 0x0130) {
        return L'i';
    }
    if (c > 0 && c <= 0xffff) {
#if OS_WIN
        return (WCHAR)(uintptr_t)CharLowerW((LPWSTR)(uintptr_t)c);
#else
        return FoldCaseWCharPortable(c);
#endif
    }
    return c;
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

// Compare needle `n` against haystack `h` for a single search "unit", case-
// folded, treating ß as equivalent to "ss". On a match returns true and reports
// how many codepoints were consumed from each side (1:1 normally, but 1:2 / 2:1 for
// the ß <-> ss equivalence). Safe to call at a string end (reads at most h[1]
// / n[1], which is the NUL terminator at worst).
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

static int StrStrFoldCase(Str haystack, int haystackLen, int startOff, Str needle, int needleLen) {
    if (!haystack || !needle) {
        return startOff;
    }
    int byteIdx = Utf8CodepointToByteIndex(haystack, startOff);
    for (int i = startOff; i < haystackLen; i++) {
        int hIdx = i;
        int hByteIdx = byteIdx;
        int nIdx = 0;
        int nByteIdx = 0;
        bool isMatch = true;
        while (nIdx < needleLen) {
            if (hIdx >= haystackLen) {
                isMatch = false;
                break;
            }
            int hAdv, nAdv, hByteAdv, nByteAdv;
            if (!MatchSearchUnit(haystack, haystackLen, hIdx, hByteIdx, needle, needleLen, nIdx, nByteIdx, hAdv, nAdv,
                                 hByteAdv, nByteAdv)) {
                isMatch = false;
                break;
            }
            hIdx += hAdv;
            nIdx += nAdv;
            hByteIdx += hByteAdv;
            nByteIdx += nByteAdv;
        }
        if (isMatch) {
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

static int StrRStr(Str text, int textLen, int endOff, Str needle, int needleLen) {
    if (!text || !needle || endOff <= 0 || endOff > textLen) {
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

static int StrRStrFoldCase(Str text, int textLen, int endOff, Str needle, int needleLen) {
    if (!text || !needle || endOff <= 0 || endOff > textLen) {
        return -1;
    }
    // ß <-> ss makes the matched length variable, so scan forward within
    // [start, end) and remember the last start position that matches.
    int result = -1;
    int byteIdx = 0;
    for (int i = 0; i < endOff; i++) {
        int hIdx = i;
        int hByteIdx = byteIdx;
        int nIdx = 0;
        int nByteIdx = 0;
        bool isMatch = true;
        while (nIdx < needleLen) {
            if (hIdx >= endOff) {
                isMatch = false;
                break;
            }
            int hAdv, nAdv, hByteAdv, nByteAdv;
            if (!MatchSearchUnit(text, textLen, hIdx, hByteIdx, needle, needleLen, nIdx, nByteIdx, hAdv, nAdv, hByteAdv,
                                 nByteAdv)) {
                isMatch = false;
                break;
            }
            hIdx += hAdv;
            nIdx += nAdv;
            hByteIdx += hByteAdv;
            nByteIdx += nByteAdv;
        }
        if (isMatch) {
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

    if (!findText) {
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
            if (!isMatch) {
                if (IsSharpS(matchCh) && !atPageEnd && endIdx + 1 < currentPageTextLen && IsLatinS(endCh)) {
                    int endAfterNextByteIdx = endNextByteIdx;
                    int nextEndCh = Utf8CodepointNext(currentPageText, endAfterNextByteIdx);
                    if (IsLatinS(nextEndCh)) {
                        // ß in the search text matches "ss" in the page
                        isMatch = true;
                        extraEndAdv = 1;
                        endNextByteIdx = endAfterNextByteIdx;
                    }
                } else if (matchIdx + 1 < findTextLen && IsLatinS(matchCh) && IsSharpS(endCh)) {
                    int matchAfterNextByteIdx = matchNextByteIdx;
                    int nextMatchCh = Utf8CodepointNext(findText, matchAfterNextByteIdx);
                    if (IsLatinS(nextMatchCh)) {
                        // "ss" in the search text matches ß in the page
                        isMatch = true;
                        extraMatchAdv = 1;
                        matchNextByteIdx = matchAfterNextByteIdx;
                    }
                }
            }
        }
        if (isMatch) {
            /* characters are identical */;
        } else if (str::IsWs((char)matchCh) && lookingAtWs) {
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
            // -> skip to next page
            ++currentPage;
            bool abortSearch = false;
            currentPageText =
                GetTextForPageForSearch(engine, currentPage, &currentPageTextLen, progressCb, &abortSearch);
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
            while (endIdx >= currentPageTextLen && currentPage < nPages) {
                // treat page break as whitespace, too
                ++currentPage;
                bool abortSearch = false;
                currentPageText =
                    GetTextForPageForSearch(engine, currentPage, &currentPageTextLen, progressCb, &abortSearch);
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

static int StrStr(Str haystack, int haystackLen, int startOff, Str needle, int needleLen) {
    if (!haystack || len(needle) == 0) {
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
    // According to my analysis of 69912675c766b6325f38036913dcf0505a00be36, when we
    // get here with pageNo != 0 the findText has already been set so I didn't add
    // a findText = engine->GetTextForPage(findPage) here.
    findPage = pageNo;

    int found = -1;
    PageAndOffset fg;
    do {
        if (WasCanceled(progressCb)) {
            return false;
        }
        if (!anchor) {
            found = GetNextIndex(pageTextLen, findIndex, forward);
        } else if (forward) {
            if (matchCase) {
                found = StrStr(pageText, pageTextLen, findIndex, anchor, anchorLen);
            } else {
                found = StrStrFoldCase(pageText, pageTextLen, findIndex, anchor, anchorLen);
            }
        } else {
            if (matchCase) {
                found = StrRStr(pageText, pageTextLen, findIndex, anchor, anchorLen);
            } else {
                found = StrRStrFoldCase(pageText, pageTextLen, findIndex, anchor, anchorLen);
            }
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
    if (result.len == 0) {
        return FindTextInPage(pageNo, finalGlyph);
    }

    if (finalGlyph) {
        *finalGlyph = fg;
    }
    return true;
}

bool TextSearch::FindStartingAtPage(int pageNo) {
    if (len(findText) == 0) {
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

        bool abortSearch = false;
        pageText = GetTextForPageForSearch(engine, pageNo, &pageTextLen, progressCb, &abortSearch);
        if (abortSearch) {
            break;
        }
        findIndex = pageTextLen;
        if (pageText) {
            if (forward) {
                findIndex = 0;
            }
            PageAndOffset r;
            if (FindTextInPage(pageNo, &r)) {
                if (forward) {
                    if (findPage != r.page) {
                        findPage = r.page;
                        pageText = GetTextForPageForSearch(engine, findPage, &pageTextLen, progressCb, &abortSearch);
                        if (abortSearch) {
                            break;
                        }
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

TextSel* TextSearch::FindFirst(int page, Str text) {
    SetText(text);

    if (FindStartingAtPage(page)) {
        return &result;
    }
    return nullptr;
}

// search only `pageNo` (no wrapping to other pages), mirroring the per-page step
// inside FindStartingAtPage. Used for page-constrained search (issue #3085)
TextSel* TextSearch::FindFirstOnPage(int pageNo, Str text) {
    SetText(text);
    if (len(findText) == 0 || pageNo < 1 || pageNo > nPages) {
        return nullptr;
    }
    Reset();
    bool abortSearch = false;
    pageText = GetTextForPageForSearch(engine, pageNo, &pageTextLen, progressCb, &abortSearch);
    if (abortSearch) {
        return nullptr;
    }
    findIndex = pageTextLen;
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
            pageText = GetTextForPageForSearch(engine, findPage, &pageTextLen, progressCb, &abortSearch);
            if (abortSearch) {
                return nullptr;
            }
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
            bool abortSearch = false;
            pageText = GetTextForPageForSearch(engine, findPage, &pageTextLen, progressCb, &abortSearch);
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
