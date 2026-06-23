/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Plain-text citation hover for PDFs without hyperref links: the engine-driven
// page walk and the per-document lookup cache that sit on top of the pure
// pattern matchers in RefHoverTextDetect. Split out of RefHover so the popup
// UI / render machinery stays separate from the citation-resolution logic.

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "RefHover.h"
#include "RefHoverText.h"
#include "RefHoverTextDetect.h"

// === Plain-text citation lookup cache ===
// Keyed by (surname, year, srcPage) so the same citation hovered repeatedly
// is resolved instantly. Negative results (citation not found) are also
// cached to avoid re-scanning the document on each hover.
struct CitationCacheEntry {
    char* surname; // owned UTF-8
    int year;
    int srcPage;  // page where the lookup was issued (so cap at srcPage works per-page)
    int destPage; // -1 if not found
    float destX;
    float destY;
};

struct RefLookupCache {
    Vec<CitationCacheEntry> entries;
};

static const CitationCacheEntry* CacheLookup(RefLookupCache* c, const char* surname, int year, int srcPage) {
    if (!c) {
        return nullptr;
    }
    for (size_t i = 0; i < c->entries.size(); i++) {
        const CitationCacheEntry& e = c->entries[i];
        if (e.year == year && e.srcPage == srcPage && str::Eq(e.surname, surname)) {
            return &e;
        }
    }
    return nullptr;
}

static void CacheInsert(RefLookupCache* c, const char* surname, int year, int srcPage, int destPage, float destX,
                        float destY) {
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
    for (size_t i = 0; i < c->entries.size(); i++) {
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
    char* surname; // owned UTF-8 (caller frees), or nullptr
    int year;
};

static void FreeDetectedCitation(DetectedCitation* c) {
    str::Free(c->surname);
    c->surname = nullptr;
}

// Detect a citation pattern under the cursor on srcPage. On success, returns
// true and fills *out with a freshly-allocated surname and year. The actual
// pattern matching is the pure DetectCitationInPageText (RefHoverDetect.cpp).
static bool DetectCitationAtCursor(EngineBase* engine, int srcPage, Point pagePos, DetectedCitation* out) {
    out->surname = nullptr;
    out->year = 0;
    int textLen = 0;
    Rect* coords = nullptr;
    const WCHAR* text = engine->GetTextForPage(srcPage, &textLen, &coords);
    return DetectCitationInPageText(text, coords, textLen, pagePos, &out->surname, &out->year);
}

// Walk pages from pageCount → srcPage looking for a bibliography entry that
// matches the surname + year. Returns true on hit.
static bool FindReferenceLocation(EngineBase* engine, int srcPage, const char* surname, int year, int* destPageOut,
                                  float* destXOut, float* destYOut) {
    if (!engine || !surname || !*surname) {
        return false;
    }
    int pageCount = engine->PageCount();
    if (pageCount <= 0 || srcPage < 1 || srcPage > pageCount) {
        return false;
    }

    // Convert surname to wide string for engine text matching.
    WCHAR* surnameW = ToWStr(surname);
    if (!surnameW) {
        return false;
    }
    int surnameLen = (int)str::Len(surnameW);
    if (surnameLen < 2) {
        free(surnameW);
        return false;
    }

    bool found = false;
    for (int p = pageCount; p >= srcPage; p--) {
        int textLen = 0;
        Rect* coords = nullptr;
        const WCHAR* text = engine->GetTextForPage(p, &textLen, &coords);
        float x = 0, y = 0;
        if (FindSurnameInPageText(text, coords, textLen, surnameW, surnameLen, year, &x, &y)) {
            *destPageOut = p;
            *destXOut = x;
            *destYOut = y;
            found = true;
            break;
        }
    }
    free(surnameW);
    return found;
}

// Look up `surname` in the cache; on miss, do a fresh document scan and
// insert the result (positive or negative). Returns true on positive hit.
static bool LookupOrSearch(RefHoverState* s, EngineBase* engine, int srcPage, const char* surname, int year,
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
    char* key = str::FormatTemp("[%d]", num);
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
        const WCHAR* text = engine->GetTextForPage(p, &textLen, &coords);
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
                          float& destXOut, float& destYOut) {
    if (!s || !engine || srcPage <= 0) {
        return false;
    }

    if (!s->lookupCache) {
        s->lookupCache = new RefLookupCache();
    }

    // Numeric "[N]" citation (IEEE / numbered reference style) — checked first
    // because the cursor sitting inside brackets is an unambiguous signal.
    {
        int textLen = 0;
        Rect* coords = nullptr;
        const WCHAR* text = engine->GetTextForPage(srcPage, &textLen, &coords);
        int num = 0;
        if (DetectNumericCitationInPageText(text, coords, textLen, pagePos, &num)) {
            if (LookupOrSearchNumeric(s, engine, srcPage, num, destPageOut, destXOut, destYOut)) {
                return true;
            }
        }
    }

    DetectedCitation cite{};
    if (!DetectCitationAtCursor(engine, srcPage, pagePos, &cite)) {
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
    if (!result && cite.surname && str::FindChar(cite.surname, ' ')) {
        struct Part {
            const char* s;
            int len;
        };
        Part parts[8];
        int nParts = 0;
        const char* p = cite.surname;
        while (*p && nParts < 8) {
            while (*p == ' ') {
                p++;
            }
            if (!*p) {
                break;
            }
            const char* start = p;
            while (*p && *p != ' ') {
                p++;
            }
            int len = (int)(p - start);
            if (len >= 2) {
                parts[nParts].s = start;
                parts[nParts].len = len;
                nParts++;
            }
        }
        // Sort parts by length descending (simple selection sort, n<=8).
        for (int i = 0; i < nParts - 1; i++) {
            for (int j = i + 1; j < nParts; j++) {
                if (parts[j].len > parts[i].len) {
                    Part t = parts[i];
                    parts[i] = parts[j];
                    parts[j] = t;
                }
            }
        }
        for (int i = 0; i < nParts && !result; i++) {
            char buf[64];
            int n = parts[i].len < 63 ? parts[i].len : 63;
            memcpy(buf, parts[i].s, n);
            buf[n] = 0;
            result = LookupOrSearch(s, engine, srcPage, buf, cite.year, destPageOut, destXOut, destYOut);
        }
    }

    FreeDetectedCitation(&cite);
    return result;
}
