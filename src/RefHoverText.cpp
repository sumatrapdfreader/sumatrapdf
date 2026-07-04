/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Plain-text citation hover for PDFs without hyperref links: the engine-driven
// page walk and the per-document lookup cache that sit on top of the pure
// pattern matchers in RefHoverTextDetect. Split out of RefHover so the popup
// UI / render machinery stays separate from the citation-resolution logic.

#include "base/Base.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHoverInternal.h"
#include "RefHoverText.h"
#include "RefHoverTextDetect.h"

TempWStr RefHoverPageTextToWStrTemp(Str text) {
    int nCodepoints = Utf8CodepointCount(text);
    WCHAR* dst = AllocArrayTemp<WCHAR>(nCodepoints + 1);
    int byteIdx = 0;
    for (int i = 0; i < nCodepoints; i++) {
        int n = 0;
        int rune = Utf8CodepointAtByte(text, byteIdx, &n);
        dst[i] = rune > 0xffff ? L'?' : (WCHAR)rune;
        byteIdx += n > 0 ? n : 1;
    }
    dst[nCodepoints] = 0;
    return WStr(dst, nCodepoints);
}

// === Plain-text citation lookup cache ===
// Keyed by (surname, year, srcPage) so the same citation hovered repeatedly
// is resolved instantly. Negative results (citation not found) are also
// cached to avoid re-scanning the document on each hover.
struct CitationCacheEntry {
    Str surname; // owned UTF-8
    int year;
    int srcPage;  // page where the lookup was issued (so cap at srcPage works per-page)
    int destPage; // -1 if not found
    float destX;
    float destY;
};

struct RefLookupCache {
    Vec<CitationCacheEntry> entries;
};

static const CitationCacheEntry* CacheLookup(RefLookupCache* c, Str surname, int year, int srcPage) {
    if (!c) {
        return nullptr;
    }
    for (int i = 0; i < len(c->entries); i++) {
        const CitationCacheEntry& e = c->entries[i];
        if (e.year == year && e.srcPage == srcPage && str::Eq(e.surname, surname)) {
            return &e;
        }
    }
    return nullptr;
}

static void CacheInsert(RefLookupCache* c, Str surname, int year, int srcPage, int destPage, float destX, float destY) {
    if (!c) {
        return;
    }
    CitationCacheEntry e;
    e.surname = str::Dup(surname);
    e.year = year;
    e.srcPage = srcPage;
    e.destPage = destPage;
    e.destX = destX;
    e.destY = destY;
    c->entries.Append(e);
}

static void CacheFree(RefLookupCache* c) {
    if (!c) {
        return;
    }
    for (int i = 0; i < len(c->entries); i++) {
        str::Free(c->entries[i].surname);
    }
    delete c;
}

// Free the lazy-init plain-text lookup cache held on the hover state.
void RefHoverFreeLookupCache(RefHoverState* s) {
    if (!s) {
        return;
    }
    CacheFree(s->lookupCache);
    s->lookupCache = nullptr;
}

// Result of detecting a citation under the cursor.
struct DetectedCitation {
    Str surname; // owned UTF-8 (caller frees)
    int year;
};

static void FreeDetectedCitation(DetectedCitation* c) {
    str::Free(c->surname);
    c->surname = {};
}

// Detect a citation pattern under the cursor on srcPage. On success, returns
// true and fills *out with a freshly-allocated surname and year. The actual
// pattern matching is the pure DetectCitationInPageText (RefHoverDetect.cpp).
static bool DetectCitationAtCursor(EngineBase* engine, int srcPage, Point pagePos, DetectedCitation* out,
                                   Rect* srcRectOut = nullptr) {
    out->surname = {};
    out->year = 0;
    int textLen = 0;
    Rect* coords = nullptr;
    Str textUtf8 = engine->GetTextForPage(srcPage, &textLen, &coords);
    TempWStr text = RefHoverPageTextToWStrTemp(textUtf8);
    return DetectCitationInPageText(text, coords, textLen, pagePos, &out->surname, &out->year, srcRectOut);
}

// Walk pages from pageCount → srcPage looking for a bibliography entry that
// matches the surname + year. Returns true on hit.
static bool FindReferenceLocation(EngineBase* engine, int srcPage, Str surname, int year, int* destPageOut,
                                  float* destXOut, float* destYOut) {
    if (!engine || !surname) {
        return false;
    }
    int pageCount = engine->PageCount();
    if (pageCount <= 0 || srcPage < 1 || srcPage > pageCount) {
        return false;
    }

    // Convert surname to wide string for engine text matching.
    WStr surnameW = ToWStr(surname);
    if (!surnameW || len(surnameW) < 2) {
        return false;
    }
    bool found = false;
    for (int p = pageCount; p >= srcPage; p--) {
        int textLen = 0;
        Rect* coords = nullptr;
        Str textUtf8 = engine->GetTextForPage(p, &textLen, &coords);
        TempWStr text = RefHoverPageTextToWStrTemp(textUtf8);
        float x = 0, y = 0;
        if (FindSurnameInPageText(text, coords, textLen, surnameW, year, &x, &y)) {
            *destPageOut = p;
            *destXOut = x;
            *destYOut = y;
            found = true;
            break;
        }
    }
    wstr::Free(surnameW);
    return found;
}

// Look up `surname` in the cache; on miss, do a fresh document scan and
// insert the result (positive or negative). Returns true on positive hit.
static bool LookupOrSearch(RefHoverState* s, EngineBase* engine, int srcPage, Str surname, int year, int& destPageOut,
                           float& destXOut, float& destYOut) {
    const CitationCacheEntry* hit = CacheLookup(s->lookupCache, surname, year, srcPage);
    if (hit) {
        if (hit->destPage > 0) {
            destPageOut = hit->destPage;
            destXOut = hit->destX;
            destYOut = hit->destY;
            return true;
        }
        return false;
    }
    int destPage = -1;
    float destX = -1.f, destY = -1.f;
    if (FindReferenceLocation(engine, srcPage, surname, year, &destPage, &destX, &destY)) {
        CacheInsert(s->lookupCache, surname, year, srcPage, destPage, destX, destY);
        destPageOut = destPage;
        destXOut = destX;
        destYOut = destY;
        return true;
    }
    CacheInsert(s->lookupCache, surname, year, srcPage, -1, 0.f, 0.f);
    return false;
}

// Numeric "[N]" citation lookup. Keyed in the same cache as author-year
// citations, using a pseudo-surname "[N]" and year=num so the two keyspaces
// never collide. On miss, scans pages pageCount → srcPage for a reference list
// line starting with "[num]".
static bool LookupOrSearchNumeric(RefHoverState* s, EngineBase* engine, int srcPage, int num, int& destPageOut,
                                  float& destXOut, float& destYOut) {
    TempStr key = fmt("[%d]", num);
    const CitationCacheEntry* hit = CacheLookup(s->lookupCache, key, num, srcPage);
    if (hit) {
        if (hit->destPage > 0) {
            destPageOut = hit->destPage;
            destXOut = hit->destX;
            destYOut = hit->destY;
            return true;
        }
        return false;
    }
    int pageCount = engine->PageCount();
    if (pageCount <= 0 || srcPage < 1 || srcPage > pageCount) {
        return false;
    }
    int destPage = -1;
    float destX = -1.f, destY = -1.f;
    bool found = false;
    for (int p = pageCount; p >= srcPage; p--) {
        int textLen = 0;
        Rect* coords = nullptr;
        Str textUtf8 = engine->GetTextForPage(p, &textLen, &coords);
        TempWStr text = RefHoverPageTextToWStrTemp(textUtf8);
        float x = 0, y = 0;
        if (FindNumericReferenceInPageText(text, coords, textLen, num, &x, &y)) {
            destPage = p;
            destX = x;
            destY = y;
            found = true;
            break;
        }
    }
    if (found) {
        CacheInsert(s->lookupCache, key, num, srcPage, destPage, destX, destY);
        destPageOut = destPage;
        destXOut = destX;
        destYOut = destY;
        return true;
    }
    CacheInsert(s->lookupCache, key, num, srcPage, -1, 0.f, 0.f);
    return false;
}

bool RefHoverTryPlainText(RefHoverState* s, EngineBase* engine, int srcPage, Point pagePos, int& destPageOut,
                          float& destXOut, float& destYOut, RectF& srcRectOut) {
    if (!s || !engine || srcPage <= 0) {
        return false;
    }
    Rect srcRect{};

    if (!s->lookupCache) {
        s->lookupCache = new RefLookupCache();
    }

    // Numeric "[N]" citation (IEEE / numbered reference style) — checked first
    // because the cursor sitting inside brackets is an unambiguous signal.
    {
        int textLen = 0;
        Rect* coords = nullptr;
        Str textUtf8 = engine->GetTextForPage(srcPage, &textLen, &coords);
        TempWStr text = RefHoverPageTextToWStrTemp(textUtf8);
        int num = 0;
        if (DetectNumericCitationInPageText(text, coords, textLen, pagePos, &num, &srcRect)) {
            if (LookupOrSearchNumeric(s, engine, srcPage, num, destPageOut, destXOut, destYOut)) {
                srcRectOut = RectF{(float)srcRect.x, (float)srcRect.y, (float)srcRect.dx, (float)srcRect.dy};
                return true;
            }
        }
    }

    DetectedCitation cite{};
    if (!DetectCitationAtCursor(engine, srcPage, pagePos, &cite, &srcRect)) {
        return false;
    }

    bool result = LookupOrSearch(s, engine, srcPage, cite.surname, cite.year, destPageOut, destXOut, destYOut);

    // Fallback: if surname has multiple space-separated parts and the full
    // form didn't match, try each part as a prefix in descending-length
    // order. Two patterns this covers:
    //   1. Bibliography lists the entry under just the last name
    //      ("Vrielink, Oude R. A." vs. detected "Oude Vrielink").
    //   2. PDF text extraction split a single-word surname by dropping a
    //      glyph ("Bash b" for "Bashab") — the longest fragment ("Bash")
    //      prefix-matches the real surname in the bibliography.
    if (!result && str::ContainsChar(cite.surname, ' ')) {
        struct Part {
            Str s;
        };
        Part parts[8];
        int nParts = 0;
        Str p = cite.surname;
        while (p && nParts < 8) {
            while (p && *p.s == ' ') {
                p = Str(p.s + 1, p.len - 1);
            }
            if (!p) {
                break;
            }
            Str start = p;
            while (p && *p.s != ' ') {
                p = Str(p.s + 1, p.len - 1);
            }
            int n = (int)(p.s - start.s);
            if (n >= 2) {
                parts[nParts].s = Str(start.s, n);
                nParts++;
            }
        }
        // Sort parts by length descending (simple selection sort, n<=8).
        for (int i = 0; i < nParts - 1; i++) {
            for (int j = i + 1; j < nParts; j++) {
                if (len(parts[j].s) > len(parts[i].s)) {
                    Part t = parts[i];
                    parts[i] = parts[j];
                    parts[j] = t;
                }
            }
        }
        for (int i = 0; i < nParts && !result; i++) {
            result = LookupOrSearch(s, engine, srcPage, parts[i].s, cite.year, destPageOut, destXOut, destYOut);
        }
    }

    if (result) {
        srcRectOut = RectF{(float)srcRect.x, (float)srcRect.y, (float)srcRect.dx, (float)srcRect.dy};
    }
    FreeDetectedCitation(&cite);
    return result;
}

float RefHoverResolveDestYFromSourceText(EngineBase* engine, int srcPage, RectF srcRect, int destPage) {
    if (srcPage <= 0 || destPage <= 0 || srcRect.dx <= 0.f || srcRect.dy <= 0.f) {
        return -1.f;
    }
    int srcLen = 0;
    Rect* srcCoords = nullptr;
    Str srcTextUtf8 = engine->GetTextForPage(srcPage, &srcLen, &srcCoords);
    TempWStr srcText = RefHoverPageTextToWStrTemp(srcTextUtf8);
    if (!srcText || srcLen <= 0 || !srcCoords) {
        return -1.f;
    }
    int srcL = (int)srcRect.x - 2;
    int srcT = (int)srcRect.y - 2;
    int srcR = (int)(srcRect.x + srcRect.dx) + 2;
    int srcB = (int)(srcRect.y + srcRect.dy) + 2;

    WCHAR rawText[512];
    int rawLen = 0;
    for (int i = 0; i < srcLen && rawLen < 511; i++) {
        Rect r = srcCoords[i];
        if (r.x + r.dx < srcL || r.x > srcR) {
            continue;
        }
        if (r.y + r.dy < srcT || r.y > srcB) {
            continue;
        }
        rawText[rawLen++] = srcText.s[i];
    }

    auto isAlnum = [](WCHAR c) {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
    };

    struct Cand {
        int start;
        int len;
        bool flanked;
    };
    constexpr int kMaxCands = 16;
    Cand cands[kMaxCands];
    int ncands = 0;
    int curStart = -1;
    int curLen = 0;
    for (int i = 0; i <= rawLen; i++) {
        bool alnum = (i < rawLen) && isAlnum(rawText[i]);
        if (alnum) {
            if (curStart < 0) {
                curStart = i;
            }
            curLen++;
        } else {
            if (curLen >= 2 && ncands < kMaxCands) {
                bool flanked = (curStart > 0 && rawText[curStart - 1] == L'(' && i < rawLen && rawText[i] == L')');
                cands[ncands++] = {curStart, curLen, flanked};
            }
            curStart = -1;
            curLen = 0;
        }
    }
    if (ncands == 0) {
        return -1.f;
    }
    for (int i = 0; i < ncands - 1; i++) {
        for (int j = i + 1; j < ncands; j++) {
            bool swap = false;
            if (cands[j].flanked && !cands[i].flanked) {
                swap = true;
            } else if (cands[j].flanked == cands[i].flanked && cands[j].len > cands[i].len) {
                swap = true;
            }
            if (swap) {
                Cand t = cands[i];
                cands[i] = cands[j];
                cands[j] = t;
            }
        }
    }

    int destLen = 0;
    Rect* destCoords = nullptr;
    Str destTextUtf8 = engine->GetTextForPage(destPage, &destLen, &destCoords);
    TempWStr destText = RefHoverPageTextToWStrTemp(destTextUtf8);
    if (!destText || destLen <= 0 || !destCoords) {
        return -1.f;
    }
    auto isLineStartMatch = [&](int idx) -> bool {
        int sy = destCoords[idx].y;
        int sx = destCoords[idx].x;
        for (int i = 0; i < destLen; i++) {
            if (i == idx) {
                continue;
            }
            if (destCoords[i].y != sy) {
                continue;
            }
            WCHAR c = destText.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            if (destCoords[i].x < sx) {
                return false;
            }
        }
        return true;
    };

    for (int ci = 0; ci < ncands; ci++) {
        int bestStart = cands[ci].start;
        int bestLen = cands[ci].len;
        auto matchAt = [&](int idx) -> bool {
            if (idx + bestLen > destLen) {
                return false;
            }
            for (int j = 0; j < bestLen; j++) {
                WCHAR a = destText.s[idx + j];
                WCHAR b = rawText[bestStart + j];
                if (a >= L'A' && a <= L'Z') {
                    a = (WCHAR)(a + 32);
                }
                if (b >= L'A' && b <= L'Z') {
                    b = (WCHAR)(b + 32);
                }
                if (a != b) {
                    return false;
                }
            }
            if (idx > 0 && isAlnum(destText.s[idx - 1])) {
                return false;
            }
            if (idx + bestLen < destLen && isAlnum(destText.s[idx + bestLen])) {
                return false;
            }
            return true;
        };

        int bestX_lineStart = INT_MAX;
        int bestY_lineStart = -1;
        int bestX_any = INT_MAX;
        int bestY_any = -1;
        for (int i = 0; i < destLen; i++) {
            if (!matchAt(i)) {
                continue;
            }
            Rect r = destCoords[i];
            if (isLineStartMatch(i)) {
                if (r.x < bestX_lineStart) {
                    bestX_lineStart = r.x;
                    bestY_lineStart = r.y;
                }
            } else if (r.x < bestX_any) {
                bestX_any = r.x;
                bestY_any = r.y;
            }
        }
        int bestY = (bestY_lineStart >= 0) ? bestY_lineStart : bestY_any;
        if (bestY >= 0) {
            return (float)bestY;
        }
    }
    return -1.f;
}
