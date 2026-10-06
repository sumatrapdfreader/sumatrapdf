/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Plain-text citation hover for PDFs without hyperref links: the engine-driven
// page walk and the per-document lookup cache that sit on top of the pure
// pattern matchers in RefHoverTextDetect. Split out of RefHover so the popup
// UI / render machinery stays separate from the citation-resolution logic.

#include "base/Base.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"

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

    ~RefLookupCache() {
        for (const auto& entry : entries) {
            str::Free(entry.surname);
        }
    }
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
    VecAppend(c->entries, CitationCacheEntry{str::Dup(surname), year, srcPage, destPage, destX, destY});
}

// Free the lazy-init plain-text lookup cache held on the hover state.
void RefHoverFreeLookupCache(RefHoverState* s) {
    if (!s) {
        return;
    }
    delete s->lookupCache;
    s->lookupCache = nullptr;
}

enum class CitationKind {
    AuthorYear,
    Number
};

// Walk pages from pageCount → srcPage looking for a bibliography entry that
// matches the surname + year. Returns true on hit.
static bool FindReferenceLocation(EngineBase* engine, int srcPage, Str surname, int year, CitationKind kind,
                                  int* destPageOut, float* destXOut, float* destYOut) {
    if (!engine || len(surname) == 0) {
        return false;
    }
    int pageCount = engine->PageCount();
    if (pageCount <= 0 || srcPage < 1 || srcPage > pageCount) {
        return false;
    }

    TempWStr surnameW = kind == CitationKind::AuthorYear ? ToWStrTemp(surname) : TempWStr{};
    if (kind == CitationKind::AuthorYear && len(surnameW) < 2) {
        return false;
    }
    for (int p = pageCount; p >= srcPage; p--) {
        int textLen = 0;
        Rect* coords = nullptr;
        Str textUtf8 = engine->GetTextForPage(p, &textLen, &coords);
        TempWStr text = RefHoverPageTextToWStrTemp(textUtf8);
        float x = 0, y = 0;
        bool found = kind == CitationKind::Number
                         ? FindNumericReferenceInPageText(text, coords, textLen, year, &x, &y)
                         : FindSurnameInPageText(text, coords, textLen, surnameW, year, &x, &y);
        if (found) {
            *destPageOut = p;
            *destXOut = x;
            *destYOut = y;
            return true;
        }
    }
    return false;
}

// Look up `surname` in the cache; on miss, do a fresh document scan and
// insert the result (positive or negative). Returns true on positive hit.
static bool LookupOrSearch(RefHoverState* s, EngineBase* engine, int srcPage, Str surname, int year, CitationKind kind,
                           int& destPageOut, float& destXOut, float& destYOut) {
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
    int pageCount = engine->PageCount();
    if (kind == CitationKind::Number && (pageCount <= 0 || srcPage < 1 || srcPage > pageCount)) {
        return false;
    }
    int destPage = -1;
    float destX = -1.f, destY = -1.f;
    if (FindReferenceLocation(engine, srcPage, surname, year, kind, &destPage, &destX, &destY)) {
        CacheInsert(s->lookupCache, surname, year, srcPage, destPage, destX, destY);
        destPageOut = destPage;
        destXOut = destX;
        destYOut = destY;
        return true;
    }
    CacheInsert(s->lookupCache, surname, year, srcPage, -1, 0.f, 0.f);
    return false;
}

// Plain-text citation hover: when no link element is under the cursor, try
// to detect a "(Surname et al., 2020)" / "Surname (2020)" pattern at pagePos
// on srcPage, find the bibliography entry that matches, and return its
// location. Returns true on success and fills destPage/destX/destY.
// Lookups are cached on s.
// srcRectOut: on success, set to a stable per-occurrence source key (page
// coords, including horizontal span) so the caller can tell two occurrences
// of the same citation apart — even on one text line — and reposition the
// popup instead of treating it as the same hover.
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
            // Numeric keys cannot collide with surnames.
            if (LookupOrSearch(s, engine, srcPage, fmt("[%d]", num), num, CitationKind::Number, destPageOut, destXOut,
                               destYOut)) {
                srcRectOut = RectF{(float)srcRect.x, (float)srcRect.y, (float)srcRect.dx, (float)srcRect.dy};
                return true;
            }
        }
    }

    int textLen = 0;
    Rect* coords = nullptr;
    TempWStr text = RefHoverPageTextToWStrTemp(engine->GetTextForPage(srcPage, &textLen, &coords));
    Str surname;
    int year = 0;
    if (!DetectCitationInPageText(text, coords, textLen, pagePos, &surname, &year, &srcRect)) {
        return false;
    }

    bool result =
        LookupOrSearch(s, engine, srcPage, surname, year, CitationKind::AuthorYear, destPageOut, destXOut, destYOut);

    // Fallback: if surname has multiple space-separated parts and the full
    // form didn't match, try each part as a prefix in descending-length
    // order. Two patterns this covers:
    //   1. Bibliography lists the entry under just the last name
    //      ("Vrielink, Oude R. A." vs. detected "Oude Vrielink").
    //   2. PDF text extraction split a single-word surname by dropping a
    //      glyph ("Bash b" for "Bashab") — the longest fragment ("Bash")
    //      prefix-matches the real surname in the bibliography.
    if (!result && str::ContainsChar(surname, ' ')) {
        constexpr int kMaxParts = 8;
        Str parts[kMaxParts];
        int nParts = 0;
        Str rest = surname;
        while (len(rest) > 0 && nParts < kMaxParts) {
            Str part;
            str::CutChar(rest, ' ', &part, &rest);
            if (len(part) >= 2) {
                parts[nParts++] = part;
            }
        }
        // Sort parts by length descending (simple selection sort, n<=8).
        for (int i = 0; i < nParts - 1; i++) {
            for (int j = i + 1; j < nParts; j++) {
                if (len(parts[j]) > len(parts[i])) {
                    std::swap(parts[i], parts[j]);
                }
            }
        }
        for (int i = 0; i < nParts && !result; i++) {
            result = LookupOrSearch(s, engine, srcPage, parts[i], year, CitationKind::AuthorYear, destPageOut, destXOut,
                                    destYOut);
        }
    }

    if (result) {
        srcRectOut = RectF{(float)srcRect.x, (float)srcRect.y, (float)srcRect.dx, (float)srcRect.dy};
    }
    str::Free(surname);
    return result;
}

// Extract the source link text and find its matching anchor on destPage.
// Best only tries the highest-ranked candidate to avoid common-word matches.
float RefHoverResolveDestYFromSourceText(EngineBase* engine, int srcPage, RectF srcRect, int destPage,
                                         RefHoverTextMatch match) {
    if (srcPage <= 0 || destPage <= 0 || srcRect.dx <= 0.f || srcRect.dy <= 0.f) {
        return -1.f;
    }
    int srcLen = 0;
    Rect* srcCoords = nullptr;
    Str srcTextUtf8 = engine->GetTextForPage(srcPage, &srcLen, &srcCoords);
    TempWStr srcText = RefHoverPageTextToWStrTemp(srcTextUtf8);
    if (len(srcText) == 0 || srcLen <= 0 || !srcCoords) {
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
    for (int i = 0; i < rawLen && ncands < kMaxCands;) {
        if (!isAlnum(rawText[i])) {
            i++;
            continue;
        }
        int start = i;
        while (i < rawLen && isAlnum(rawText[i])) {
            i++;
        }
        int count = i - start;
        if (count < 2) {
            continue;
        }
        bool flanked = start > 0 && rawText[start - 1] == L'(' && i < rawLen && rawText[i] == L')';
        cands[ncands++] = {start, count, flanked};
    }
    if (ncands == 0) {
        return -1.f;
    }
    for (int i = 0; i < ncands - 1; i++) {
        for (int j = i + 1; j < ncands; j++) {
            bool swap = (cands[j].flanked && !cands[i].flanked) ||
                        (cands[j].flanked == cands[i].flanked && cands[j].len > cands[i].len);
            if (swap) {
                std::swap(cands[i], cands[j]);
            }
        }
    }

    int destLen = 0;
    Rect* destCoords = nullptr;
    Str destTextUtf8 = engine->GetTextForPage(destPage, &destLen, &destCoords);
    TempWStr destText = RefHoverPageTextToWStrTemp(destTextUtf8);
    if (len(destText) == 0 || destLen <= 0 || !destCoords) {
        return -1.f;
    }
    auto isLineStartMatch = [&](int idx) -> bool {
        int sy = destCoords[idx].y;
        int sx = destCoords[idx].x;
        for (int i = 0; i < destLen; i++) {
            if (i == idx || destCoords[i].y != sy) {
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

        Point lineStart{INT_MAX, -1};
        Point other{INT_MAX, -1};
        for (int i = 0; i < destLen; i++) {
            if (!matchAt(i)) {
                continue;
            }
            Rect r = destCoords[i];
            Point& best = isLineStartMatch(i) ? lineStart : other;
            if (r.x < best.x) {
                best = Point(r.x, r.y);
            }
        }
        int bestY = lineStart.y >= 0 ? lineStart.y : other.y;
        if (bestY >= 0) {
            return (float)bestY;
        }
        if (match == RefHoverTextMatch::Best) {
            break;
        }
    }
    return -1.f;
}
