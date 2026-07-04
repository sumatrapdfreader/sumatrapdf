/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function popup-region detectors used by RefHover. Kept in a separate
// translation unit so the heuristics can be unit-tested with synthetic glyph
// arrays (see src/base/tests/RefHover_ut.cpp) without pulling in the engine,
// HWND, or rendering layers.

#include "base/Base.h"

#include <wctype.h>

static constexpr float kAnchorTopMarginPt = 6.f;
// pt of padding around the detected entry box.
static constexpr float kEntryPadPt = 6.f;

static bool IsAsciiAlnum(WCHAR c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
}

// Locale-independent lowercasing. The process runs in the "C" locale where
// towlower() only folds ASCII (as does ToLowerW() in src/common), so
// accented dictionary words ("sección", "capítulo") would never match
// all-caps headings ("SECCIÓN 2").
static WCHAR FoldCaseW(WCHAR c) {
    return (WCHAR)(uintptr_t)CharLowerW((LPWSTR)(uintptr_t)c);
}

// Caption / heading keyword tables, \0-separated utf8 strings. Each entry
// is a lowercase word recognised at the start of a "Figure 1.2" /
// "Tableau 2" style label. Add a language by appending entries; the call
// sites loop the table so no other code changes.
//
// Entries are matched case-insensitively against the input glyph (via
// CharLowerW, which folds accented letters regardless of the CRT locale)
// so capitalised or all-caps PDF text matches too. Accented dict words
// must be stored already-lowercased (NFC) — PDF text extraction produces
// NFC most of the time.
// clang-format off
static SeqStrings gCaptionWords =
    // en
    "figure\0" "table\0" "listing\0" "algorithm\0"
    // de
    "abbildung\0" "tabelle\0" "algorithmus\0"
    // es / it / pt (shared roots: figura, algoritmo)
    "figura\0" "algoritmo\0"
    // es
    "tabla\0"
    // it
    "tabella\0"
    // pt
    "tabela\0"
    // fr
    "tableau\0" "algorithme\0";
// clang-format on

// Heading prefixes recognised at the start of a destination glyph run. Used
// to disambiguate a section heading / figure caption destination from a
// description-list bibliography entry. Superset of gCaptionWords — includes
// "section" / "chapter" / locale equivalents that aren't captions but are
// heading destinations.
// clang-format off
static SeqStrings gHeadingPrefixWords =
    // en
    "figure\0" "table\0" "listing\0" "section\0" "chapter\0" "algorithm\0"
    // de
    "abbildung\0" "tabelle\0" "abschnitt\0" "kapitel\0" "algorithmus\0"
    // es / it / pt (shared roots: figura, algoritmo; capítulo is es + pt)
    "figura\0" "algoritmo\0" "capítulo\0"
    // es
    "tabla\0" "sección\0"
    // it
    "tabella\0" "sezione\0" "capitolo\0"
    // pt
    "tabela\0" "seção\0" "secção\0"
    // fr
    "tableau\0" "chapitre\0" "algorithme\0";
// clang-format on

// Match `text[idx..]` case-insensitively against the lowercase dictionary
// word `w`. Returns true on full word match. When requireTrailingDigit is
// set, also requires optional whitespace then a digit immediately after the
// word (the "Figure 1.2" trailing-number constraint).
static bool MatchWordAt(WStr text, int idx, WStr w, bool requireTrailingDigit) {
    int n = w.len;
    if (idx + n > text.len) {
        return false;
    }
    if (requireTrailingDigit && idx + n + 1 >= text.len) {
        return false;
    }
    for (int j = 0; j < n; j++) {
        WCHAR c = FoldCaseW(text.s[idx + j]);
        if (c != w.s[j]) {
            return false;
        }
    }
    if (!requireTrailingDigit) {
        // require a trailing word boundary so that e.g. "Sections of ..."
        // doesn't match "section" or "Tableaux ..." match "tableau"
        if (idx + n < text.len) {
            WCHAR next = text.s[idx + n];
            if ((next >= L'a' && next <= L'z') || (next >= L'A' && next <= L'Z')) {
                return false;
            }
        }
        return true;
    }
    int k = idx + n;
    while (k < text.len && (text.s[k] == L' ' || text.s[k] == L'\t')) {
        k++;
    }
    return k < text.len && text.s[k] >= L'0' && text.s[k] <= L'9';
}

// True if `text[idx..]` starts a caption label like "Figure 1.2", "Tableau 2".
// See kCaptionWords for the language list. Requires the previous glyph to be
// a word boundary and the word to be followed by whitespace and a digit.
static bool IsCaptionLabelAt(WStr text, int idx) {
    if (idx > 0 && IsAsciiAlnum(text.s[idx - 1])) {
        return false;
    }
    for (int off = 0; SeqStrAt(gCaptionWords, off);) {
        TempWStr w = ToWStrTemp(SeqStrAt(gCaptionWords, off));
        if (MatchWordAt(text, idx, w, /*requireTrailingDigit=*/true)) {
            return true;
        }
        if (!SeqStrAdvance(gCaptionWords, off)) {
            break;
        }
    }
    return false;
}

// Clip a region to the page mediabox: shifts a negative x/y to 0 (shrinking
// the box by the same amount) and trims any overhang past the right/bottom
// edge. Used everywhere a detected region must be passed to RenderPage.
static void ClipToMediabox(RectF& box, RectF mediabox) {
    if (box.x < 0.f) {
        box.dx += box.x;
        box.x = 0.f;
    }
    if (box.y < 0.f) {
        box.dy += box.y;
        box.y = 0.f;
    }
    if (box.x + box.dx > mediabox.dx) {
        box.dx = mediabox.dx - box.x;
    }
    if (box.y + box.dy > mediabox.dy) {
        box.dy = mediabox.dy - box.y;
    }
}

// Snap glyphs to text lines and rewrite each glyph's y/dy to its line's
// top/height. mupdf returns tight per-glyph ink boxes, so glyphs on one
// visual line have *different* tops (a period or comma sits well below a
// capital, an ascender above it). Every line heuristic below treats
// coords[i].y as the line's position (grouping by y±tol), which a stray
// low-topped glyph from an adjacent line defeats — e.g. the trailing "."
// of the previous bibliography entry landing inside the destination band and
// hijacking the entry-start search. The glyph *baseline* (y + dy) is stable
// across a line (a digit and a period share it), so cluster by baseline and
// flatten each line to a uniform top-aligned row — the shape the detectors
// assume. `out` must have room for glyphCount rects; aliasing `coords` is not
// allowed.
void NormalizeGlyphLines(const Rect* coords, Rect* out, int glyphCount) {
    if (!coords || !out || glyphCount <= 0) {
        return;
    }
    constexpr int kBaselineTolPt = 4;
    constexpr int kMaxLines = 4096;
    int* lineBaseline = AllocArray<int>(kMaxLines);
    int* lineTop = AllocArray<int>(kMaxLines);
    int* lineBottom = AllocArray<int>(kMaxLines);
    int* lineId = AllocArray<int>(glyphCount);
    int nLines = 0;
    for (int i = 0; i < glyphCount; i++) {
        int bl = coords[i].y + coords[i].dy;
        int best = -1;
        int bestDist = kBaselineTolPt + 1;
        for (int L = 0; L < nLines; L++) {
            int dist = bl - lineBaseline[L];
            if (dist < 0) {
                dist = -dist;
            }
            if (dist < bestDist) {
                bestDist = dist;
                best = L;
            }
        }
        if (best < 0) {
            // new line (or, on the unlikely line overflow, fold into line 0)
            if (nLines < kMaxLines) {
                best = nLines++;
                lineBaseline[best] = bl;
                lineTop[best] = coords[i].y;
                lineBottom[best] = bl;
            } else {
                best = 0;
            }
        } else {
            if (coords[i].y < lineTop[best]) {
                lineTop[best] = coords[i].y;
            }
            if (bl > lineBottom[best]) {
                lineBottom[best] = bl;
            }
        }
        lineId[i] = best;
    }
    for (int i = 0; i < glyphCount; i++) {
        out[i] = coords[i];
        int L = lineId[i];
        out[i].y = lineTop[L];
        out[i].dy = lineBottom[L] - lineTop[L];
    }
    free(lineBaseline);
    free(lineTop);
    free(lineBottom);
    free(lineId);
}

// Drop diagonal draft / "under review" watermark glyphs from a page's *raw*
// glyph arrays before any box detection. A watermark stamp is set far larger
// than body text and, being rotated, sits roughly one glyph per baseline —
// each on a sparse row — whereas a heading or title is a horizontal run of
// same-baseline glyphs. Removing it up front keeps a 2-column gutter empty and
// the entry bounds tight, instead of special-casing oversized glyphs in every
// scan. Run this on the engine's raw coords *before* NormalizeGlyphLines:
// normalization clusters by baseline (±4pt) and could fold a watermark glyph
// into a body line, hiding its true height.
//
// `outText`/`outCoords` are caller-allocated with room for glyphCount entries;
// returns the number of glyphs kept (written to the front of the out arrays).
int StripWatermarkGlyphs(WStr text, const Rect* coords, WCHAR* outText, Rect* outCoords) {
    int n = text.len;
    if (n <= 0 || !coords || !outText || !outCoords) {
        return 0;
    }
    // Typical body glyph height = the most common dy (the watermark, a heading,
    // and any super/subscripts are all minorities). Histogram over non-space
    // glyph heights and take the mode.
    int maxDy = 0;
    for (int i = 0; i < n; i++) {
        if (coords[i].dy > maxDy) {
            maxDy = coords[i].dy;
        }
    }
    int modeDy = 0;
    if (maxDy > 0) {
        int* hist = AllocArray<int>(maxDy + 1);
        for (int i = 0; i < n; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            int d = coords[i].dy;
            if (d > 0) {
                hist[d]++;
            }
        }
        int modeCount = 0;
        for (int d = 1; d <= maxDy; d++) {
            if (hist[d] > modeCount) {
                modeCount = hist[d];
                modeDy = d;
            }
        }
        free(hist);
    }

    // Only strip when there's a stable body height to compare against, and only
    // glyphs clearly taller than it (1.5x) — well above tall "[" labels / caps.
    constexpr int kMinBodyDy = 4;
    bool canStrip = modeDy >= kMinBodyDy;
    int hgtThresh = modeDy + modeDy / 2; // 1.5 * modeDy
    constexpr int kBaselineTolPt = 4;
    constexpr int kMinRowGlyphs = 3; // a real text row has at least this many

    int outLen = 0;
    for (int i = 0; i < n; i++) {
        WCHAR c = text.s[i];
        bool isSpace = (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r');
        bool drop = false;
        if (canStrip && !isSpace && coords[i].dy > hgtThresh) {
            // Sparse-row test: count non-space glyphs sharing this glyph's
            // baseline (y+dy, stable across a visual line) AND of comparable
            // height. A rotated watermark glyph stands nearly alone on its
            // baseline; a heading is a dense row of same-size glyphs. Requiring
            // *similar height* also catches a watermark glyph whose baseline
            // happens to coincide with a body line — it's then the lone tall
            // glyph on a row of small body text, not one of a tall row.
            int bl = coords[i].y + coords[i].dy;
            int hi = coords[i].dy;
            int rowGlyphs = 0;
            for (int j = 0; j < n; j++) {
                WCHAR cj = text.s[j];
                if (cj == L' ' || cj == L'\t' || cj == L'\n' || cj == L'\r') {
                    continue;
                }
                if (abs((coords[j].y + coords[j].dy) - bl) > kBaselineTolPt) {
                    continue;
                }
                if (abs(coords[j].dy - hi) * 2 > hi) { // height differs by > 50%
                    continue;
                }
                rowGlyphs++;
                if (rowGlyphs >= kMinRowGlyphs) {
                    break;
                }
            }
            if (rowGlyphs < kMinRowGlyphs) {
                drop = true;
            }
        }
        if (drop) {
            continue;
        }
        outText[outLen] = c;
        outCoords[outLen] = coords[i];
        outLen++;
    }
    return outLen;
}

// Used when the link doesn't resolve to a recognizable bibliography entry —
// TOC targets, topbar/section links, table or figure captions, image-only
// PDFs. Returns a region that spans the full page width and goes from the
// destination Y down to the bottom of the last text glyph on the page —
// captures table caption / figure caption / section content below the
// link target without leaving a long blank margin at the popup bottom.
// Auto-fit in RefHoverOnTimer + the monitor-based popup height cap keep
// the popup a sensible size; the user can wheel-zoom in if text is too
// small.
RectF LandscapeBox(RectF mediabox, float destX, float destY, WStr text, const Rect* coords) {
    (void)destX;
    float ty = (destY >= 0.f) ? destY - kAnchorTopMarginPt : 0.f;
    if (ty < 0.f) {
        ty = 0.f;
    }
    // When destY anchors at a "Figure N.M" / "Abbildung N.M" / "Table N.M"
    // caption line, the figure / table *body* sits above the caption — but
    // ty currently starts at the caption. Extend upward so the popup
    // includes the figure body, not just the caption + the paragraph
    // following it.
    bool destAtCaption = false;
    if (len(text) > 0 && coords && destY > 0.f) {
        int dY = (int)destY;
        for (int i = 0; i < text.len; i++) {
            int gy = coords[i].y;
            if (gy < dY - 5 || gy > dY + 15) {
                continue;
            }
            if (IsCaptionLabelAt(text, i)) {
                destAtCaption = true;
                break;
            }
        }
    }
    if (destAtCaption) {
        constexpr float kFigureBodyExtendPt = 250.f;
        float newTy = ty - kFigureBodyExtendPt;
        if (newTy < 0.f) {
            newTy = 0.f;
        }
        ty = newTy;
    }
    float h = mediabox.dy - ty;
    if (h <= 0.f) {
        h = mediabox.dy;
        ty = 0.f;
    }
    // Cap to a focused region size so the popup is wide and short rather
    // than narrow and tall. Captions get a taller cap so the figure body
    // above and the caption text below both fit.
    constexpr float kMaxLandscapePt = 200.f;
    constexpr float kMaxLandscapeCaptionPt = 360.f;
    float maxLandscape = destAtCaption ? kMaxLandscapeCaptionPt : kMaxLandscapePt;
    if (h > maxLandscape) {
        h = maxLandscape;
    }
    // Caption extension: if a "Figure N.M" / "Table N.M" / "Listing N.M" /
    // "Algorithm N.M" caption appears within ~250pt below the capped region
    // bottom (typical figure body height), extend the region downward to
    // include the full caption block. Necessary for image-only figures
    // where the figure body has no extractable text at destY — the caller
    // falls to LandscapeBox without ever running the caption-aware
    // DetectEntryBox path.
    if (len(text) > 0 && coords) {
        // Search to end of page so tall figures with captions far below the
        // initial 200pt cap still match. The topmost (smallest y) "Figure
        // N.M" below the cap wins — PDFs draw text in arbitrary order, so
        // the first label in glyph-array order can be a caption much
        // further down the page.
        int searchTop = (int)(ty + h);
        int searchBot = (int)mediabox.dy;
        int capStartIdx = -1;
        int capBestY = INT_MAX;
        for (int i = 0; i < text.len; i++) {
            int gy = coords[i].y;
            if (gy < searchTop || gy > searchBot || gy >= capBestY) {
                continue;
            }
            if (IsCaptionLabelAt(text, i)) {
                capStartIdx = i;
                capBestY = gy;
            }
        }
        if (capStartIdx >= 0) {
            int capStartY = coords[capStartIdx].y;
            int capLineH = coords[capStartIdx].dy;
            if (capLineH < 10) {
                capLineH = 12;
            }
            // Page right text margin: max right-X across all text glyphs on
            // the page. A line reaching within ~30pt of pageRightX is at the
            // column edge (justified body, or a hyphenated caption line).
            int pageRightX = 0;
            for (int j = 0; j < text.len; j++) {
                int rx = coords[j].x + coords[j].dx;
                if (rx > pageRightX) {
                    pageRightX = rx;
                }
            }
            // Walk subsequent lines below capStartY. Stop when we hit a
            // paragraph break (vertical gap above inter-line leading) or
            // a body-shape line. Two signals to detect body:
            //   1) gap > ~70% of capLineH (parskip / float-separator) =
            //      new paragraph.
            //   2) a "short" caption line seen earlier and the current line
            //      fills the column (raggedright-then-justified transition).
            // Either signal alone catches a common case; together they cover
            // hyphenated multi-line German captions (e.g. "...Bo-/gner...")
            // where every caption line happens to reach the right margin.
            int captionEndY = capStartY + capLineH;
            int prevLineBottom = capStartY + capLineH - 1;
            bool seenShortLine = false;
            for (int lineIdx = 0; lineIdx < 3; lineIdx++) {
                int capTop, capBot;
                if (lineIdx == 0) {
                    capTop = capStartY - 3;
                    capBot = capStartY + 3;
                } else {
                    capTop = prevLineBottom + 1;
                    capBot = prevLineBottom + capLineH * 18 / 10;
                }
                bool foundLine = false;
                int lineTopY = INT_MAX;
                int lineBottomY = -1;
                int lineRightX = 0;
                for (int j = 0; j < text.len; j++) {
                    int gy = coords[j].y;
                    if (gy < capTop || gy > capBot) {
                        continue;
                    }
                    foundLine = true;
                    if (gy < lineTopY) {
                        lineTopY = gy;
                    }
                    int gb = gy + coords[j].dy;
                    if (gb > lineBottomY) {
                        lineBottomY = gb;
                    }
                    int rx = coords[j].x + coords[j].dx;
                    if (rx > lineRightX) {
                        lineRightX = rx;
                    }
                }
                if (!foundLine) {
                    break;
                }
                bool isShort = lineRightX < pageRightX - 30;
                if (lineIdx >= 1) {
                    int gap = lineTopY - prevLineBottom;
                    if (gap > capLineH * 7 / 10) {
                        break;
                    }
                    if (!isShort && seenShortLine) {
                        break;
                    }
                }
                captionEndY = lineBottomY;
                prevLineBottom = lineBottomY;
                if (isShort) {
                    seenShortLine = true;
                }
            }
            float extendedH = (float)captionEndY + kAnchorTopMarginPt - ty;
            if (extendedH > h) {
                h = extendedH;
            }
        }
    }
    // Trim trailing blank margin: find the bottom of the last text glyph
    // inside the candidate region and end the region just below it so the
    // popup doesn't render an empty trailing margin.
    if (len(text) > 0 && coords) {
        int boxTop = (int)ty;
        int boxBottom = (int)(ty + h);
        int lastTextBottom = boxTop;
        for (int i = 0; i < text.len; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.y < boxTop || r.y >= boxBottom) {
                continue;
            }
            int glyphBottom = r.y + r.dy;
            if (glyphBottom > lastTextBottom) {
                lastTextBottom = glyphBottom;
            }
        }
        float trimmedH = (float)lastTextBottom + kAnchorTopMarginPt - ty;
        if (trimmedH > 20.f && trimmedH < h) {
            h = trimmedH;
        }
    }
    return RectF{0.f, ty, mediabox.dx, h};
}

// Detect a labelled display equation at (destX, destY): a "(N)" or "(N.M)"
// glyph cluster sitting near the right column edge on or near destY, with
// no other text further right on that line. Returns the equation's tight
// bounding box (full page width, ~one eq line tall) when found, empty rect
// otherwise. Used to avoid the landscape-style 200pt slice that sweeps in
// the paragraph and the next equation below an equation cross-reference.
RectF DetectEquationBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY) {
    (void)destX;
    RectF empty{};
    if (destY <= 0.f || len(text) == 0 || !coords) {
        return empty;
    }
    int dY = (int)destY;

    // Scan glyphs in a band around destY. Find a ')' whose right edge is the
    // rightmost in its line, preceded by digits and an opening '('.
    int bestLabelY = -1;
    int bestLabelDy = 0;
    int bestDist = INT_MAX;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] != L')') {
            continue;
        }
        int ly = coords[i].y;
        if (ly < dY - 40 || ly > dY + 40) {
            continue;
        }
        // Walk backward through digits on the same line.
        int p = i - 1;
        int digits = 0;
        while (p >= 0 && wstr::IsDigit(text.s[p]) && coords[p].y == ly) {
            p--;
            digits++;
        }
        if (digits == 0) {
            continue;
        }
        // Optional ".M" form.
        bool hadDot = false;
        if (p >= 0 && text.s[p] == L'.' && coords[p].y == ly) {
            hadDot = true;
            p--;
            int d2 = 0;
            while (p >= 0 && wstr::IsDigit(text.s[p]) && coords[p].y == ly) {
                p--;
                d2++;
            }
            if (d2 == 0) {
                continue;
            }
        }
        if (p < 0 || text.s[p] != L'(' || coords[p].y != ly) {
            continue;
        }
        // Reject a 4-digit "(YYYY)" — a citation year at the end of a
        // bibliography line ("... IGI Global (2013).") is not a display-equation
        // label. Real equation numbers are 1-3 digits or an "N.M" form; a plain
        // 4-digit parenthesised number in a reference list is a year, and
        // matching it would render the whole (full-width) reference row.
        if (!hadDot && digits >= 4) {
            continue;
        }
        int labelLeftX = coords[p].x;
        int labelRightX = coords[i].x + coords[i].dx;
        // Reject if any non-space glyph on the same line sits further right
        // than the label — equation labels are line-trailing by construction.
        bool hasRightOf = false;
        for (int j = 0; j < text.len; j++) {
            if (j >= p && j <= i) {
                continue;
            }
            if (wstr::IsWs(text.s[j])) {
                continue;
            }
            if (coords[j].y != ly) {
                continue;
            }
            if (coords[j].x + coords[j].dx > labelRightX) {
                hasRightOf = true;
                break;
            }
        }
        if (hasRightOf) {
            continue;
        }
        // Reject if the label sits in the left half of the page (likely a
        // body-text "(N)" footnote marker, not a display-eq label).
        if (labelLeftX < (int)(mediabox.dx * 0.5f)) {
            continue;
        }
        int dist = std::abs(ly - dY);
        if (dist < bestDist) {
            bestDist = dist;
            bestLabelY = ly;
            bestLabelDy = coords[i].dy;
        }
    }
    if (bestLabelY < 0) {
        return empty;
    }
    if (bestLabelDy <= 0) {
        bestLabelDy = 12;
    }
    // Region: one eq line — labeled row + small vertical padding. Multi-row
    // align environments are rare in cross-refs; a tight box is the right
    // default and the user can wheel-scroll if context is needed.
    float pad = (float)bestLabelDy + 6.f;
    RectF box{0.f, (float)bestLabelY - pad, mediabox.dx, (float)bestLabelDy + 2.f * pad};
    ClipToMediabox(box, mediabox);
    return box;
}

// Horizontal extent of the run of glyphs on the same line (±3pt) as
// anchorIdx, expanding left / right glyph by glyph but never across a
// horizontal gap wider than kMaxLineGapPt. In multi-column layouts this
// keeps the run within one column: gutters are wider than spacing within a
// line. (Tradeoff: a description-list label separated from its body by more
// than the threshold isn't reached; the bracket-label search in
// DetectEntryBox recovers the common "[Foo09]" case.)
static void LineRunExtent(WStr text, const Rect* coords, int anchorIdx, int* leftIdxOut, int* leftXOut,
                          int* rightXOut) {
    constexpr int kMaxLineGapPt = 20;
    int sy = coords[anchorIdx].y;
    int leftIdx = anchorIdx;
    int leftX = coords[anchorIdx].x;
    int rightX = coords[anchorIdx].x + coords[anchorIdx].dx;
    bool extended = true;
    while (extended) {
        extended = false;
        for (int i = 0; i < text.len; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.y < sy - 3 || r.y > sy + 3) {
                continue;
            }
            if (r.x < leftX && r.x + r.dx >= leftX - kMaxLineGapPt) {
                leftX = r.x;
                leftIdx = i;
                extended = true;
            }
            if (r.x + r.dx > rightX && r.x <= rightX + kMaxLineGapPt) {
                rightX = r.x + r.dx;
                extended = true;
            }
        }
    }
    *leftIdxOut = leftIdx;
    *leftXOut = leftX;
    *rightXOut = rightX;
}

// A bracket-style bibliography entry ("[63]") that runs to the bottom of its
// 2-column-layout column with no sibling "[" and no blank-line gap closing it
// may simply continue at the top of the next column (the column break falls
// mid-entry). Look for that continuation: a block of body text starting at
// the top of the column right of `oldColumnRightX` that does *not* itself
// begin with a "[" label (which would mean it's the next real entry, not a
// continuation). Returns an empty RectF when no such continuation is found.
static RectF FindColumnWrapContinuation(WStr text, const Rect* coords, RectF mediabox, int oldColumnRightX) {
    constexpr int kGutterW = 8;

    // 1. Left edge of the next column: leftmost glyph right of the old
    // column's right edge (skipping the gutter itself).
    int nextColLeftX = INT_MAX;
    for (int i = 0; i < text.len; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        if (r.x > oldColumnRightX + kGutterW && r.x < nextColLeftX) {
            nextColLeftX = r.x;
        }
    }
    if (nextColLeftX == INT_MAX) {
        return RectF{};
    }

    // 2. Topmost line in the next column, and its leftmost X (candidate
    // continuation start). Skip lines that bridge across the whole page width
    // (a running header/title above both columns), and skip anything sitting
    // in the page's top margin — a running header (page number, journal
    // title) commonly renders as several short, column-confined fragments
    // rather than one wide line, so the page-width check alone doesn't catch
    // it. Real column body content essentially never starts this close to
    // the physical page edge.
    constexpr int kColWidthMax = 280;
    constexpr int kMinTopMarginPt = 30;
    int topY = INT_MAX;
    for (int i = 0; i < text.len; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        if (r.x < nextColLeftX - 5 || r.y < kMinTopMarginPt) {
            continue;
        }
        int leftIdx, leftX, rightX;
        LineRunExtent(text, coords, i, &leftIdx, &leftX, &rightX);
        if (rightX - leftX > kColWidthMax || leftX < nextColLeftX - 20) {
            continue;
        }
        if (r.y < topY) {
            topY = r.y;
        }
    }
    if (topY == INT_MAX) {
        return RectF{};
    }
    int topLeftX = INT_MAX;
    int topDy = 12;
    for (int i = 0; i < text.len; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        if (r.x < nextColLeftX - 5 || r.y < topY - 3 || r.y > topY + 3) {
            continue;
        }
        if (r.x < topLeftX) {
            topLeftX = r.x;
            topDy = r.dy > 0 ? r.dy : 12;
        }
    }

    // 3. Reject: the top line is itself a new entry's "[" label, not a
    // continuation of the previous one.
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] != L'[') {
            continue;
        }
        Rect r = coords[i];
        if (r.y >= topY - 3 && r.y <= topY + 3 && r.x >= topLeftX - 3 && r.x <= topLeftX + 3) {
            return RectF{};
        }
    }

    // 4. Right edge of the new column (gutter-bounded, mirrors the primary
    // column-right scan in DetectEntryBox).
    int colRightX = nextColLeftX + 250;
    {
        int bandTop = topY - 2;
        int bandBot = topY + 6 * topDy;
        int xLo = nextColLeftX - 5;
        if (xLo < 0) {
            xLo = 0;
        }
        int xHi = (int)mediabox.dx;
        if (xHi > xLo + 2) {
            int n = xHi - xLo;
            char* occ = AllocArray<char>(n);
            for (int i = 0; i < text.len; i++) {
                WCHAR c = text.s[i];
                if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                    continue;
                }
                Rect r = coords[i];
                if (r.y < bandTop || r.y > bandBot) {
                    continue;
                }
                int a = r.x - xLo;
                int b = r.x + r.dx - xLo;
                if (b <= 0 || a >= n) {
                    continue;
                }
                if (a < 0) {
                    a = 0;
                }
                if (b > n) {
                    b = n;
                }
                for (int x = a; x < b; x++) {
                    occ[x] = 1;
                }
            }
            int lastOcc = nextColLeftX;
            for (int x = nextColLeftX; x < xHi; x++) {
                int idx = x - xLo;
                if (idx >= 0 && idx < n && occ[idx]) {
                    lastOcc = x + 1;
                } else if (x - lastOcc >= kGutterW) {
                    break;
                }
            }
            free(occ);
            if (lastOcc > nextColLeftX) {
                colRightX = lastOcc;
            }
        }
    }

    // 5. End of the continuation block. A real wrapped tail is short (finishes
    // a sentence + a citation line or two), so cap the search tight — much
    // tighter than a full entry's height cap in the primary scan above.
    // Content that isn't closed by a sibling "[" (the following real entry)
    // within that short cap is something else entirely (e.g. running body
    // text that happens to share the column, ending in an unrelated "["
    // many lines down) — reject rather than grab an arbitrary slice of it.
    constexpr int kMaxContinuationPt = 60;
    int capY = topY + kMaxContinuationPt;
    int boundaryY = capY;
    bool closedBySibling = false;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] != L'[') {
            continue;
        }
        Rect r = coords[i];
        if (r.x < nextColLeftX - 5 || r.x > nextColLeftX + 30) {
            continue;
        }
        if (r.y <= topY + topDy / 2 || r.y >= capY) {
            continue;
        }
        if (r.y < boundaryY) {
            boundaryY = r.y;
            closedBySibling = true;
        }
    }
    if (!closedBySibling) {
        // No sibling closed it within the cap. Still acceptable if the column
        // has no more text at all past the cap (a short trailing tail that's
        // simply the last thing in the column); reject if text keeps flowing
        // past the cap, since that's not a short wrap.
        for (int i = 0; i < text.len; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.x < nextColLeftX - 20 || r.x > colRightX) {
                continue;
            }
            if (r.y >= capY - topDy) {
                return RectF{};
            }
        }
    }

    // 6. Bounding box of the continuation block's glyphs.
    int bMinX = INT_MAX, bMinY = INT_MAX, bMaxX = INT_MIN, bMaxY = INT_MIN;
    for (int i = 0; i < text.len; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        if (r.x < nextColLeftX - 20 || r.x > colRightX) {
            continue;
        }
        if (r.y < topY - 5 || r.y >= boundaryY) {
            continue;
        }
        if (r.x < bMinX) {
            bMinX = r.x;
        }
        if (r.y < bMinY) {
            bMinY = r.y;
        }
        if (r.x + r.dx > bMaxX) {
            bMaxX = r.x + r.dx;
        }
        if (r.y + r.dy > bMaxY) {
            bMaxY = r.y + r.dy;
        }
    }
    if (bMinX == INT_MAX || (bMaxX - bMinX) < 30 || (bMaxY - bMinY) < 8) {
        return RectF{};
    }
    RectF box{(float)bMinX - kEntryPadPt, (float)bMinY - kEntryPadPt, (float)(bMaxX - bMinX) + 2.f * kEntryPadPt,
              (float)(bMaxY - bMinY) + 2.f * kEntryPadPt};
    ClipToMediabox(box, mediabox);
    return box;
}

// Find the bounding box of a single bibliography entry on the destination
// page. Uses per-glyph text+coords from the engine's text cache:
//   1. Locate the leftmost glyph with y in a small band around destY (entry start).
//   2. Scan forward; stop at "[N" near the same left margin (next entry) or
//      a vertical paragraph gap.
//   3. Return the min/max bounding box of glyphs in [start, end), padded.
// Falls back to LandscapeBox() when the link is not a bibliography reference
// (TOC, topbar, cross-ref, table caption). The landscape box renders a half-
// page-tall slice of the page anchored on the destination so the user sees
// surrounding context (e.g. the table rows under a caption).
RectF DetectEntryBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY, RectF* continuationOut) {
    if (continuationOut) {
        *continuationOut = RectF{};
    }
    // Sparse-text dest page (image-only or near-image-only — e.g. a
    // children's PDF overview with character thumbnails plus a single
    // heading). Fitting to the heading line gives a thin sliver and hides
    // the actual content. Show the whole page so the user sees what they
    // would navigate to; the auto-fit in RefHoverOnTimer scales the bitmap
    // to popup limits.
    constexpr int kSparsePageTextLen = 50;
    if (!text || text.len < kSparsePageTextLen || !coords) {
        return RectF{0.f, 0.f, mediabox.dx, mediabox.dy};
    }
    if (destY < 0.f) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }

    int dY = (int)destY;
    int dX = (int)destX;
    // Constrain to the destination's column — for 2-column layouts this
    // prevents the search from latching onto same-Y body text in another
    // column. We allow a small left tolerance so a "[1]" whose [ starts
    // a few pt left of destX still matches.
    int columnLeft = (destX >= 0.f) ? dX - 15 : INT_MIN;

    // 1. Find the start glyph: the non-whitespace glyph on the line nearest
    //    destY (within [destY-5, destY+30]) and at-or-right-of columnLeft,
    //    tie-broken by leftmost x. Selecting by nearness to destY (rather than
    //    the globally-topmost line in the window) keeps the start on the
    //    destination's own entry: in a 2-column reference list, a neighbouring
    //    column's line a few pt above destY would otherwise win the topmost
    //    pick (columnLeft only bounds the left side, so the other column's
    //    larger x still passes) and the popup would render the wrong column.
    int startIdx = -1;
    int bestDistY = INT_MAX;
    int bestX = INT_MAX;
    for (int i = 0; i < text.len; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        if (r.y < dY - 5 || r.y > dY + 30) {
            continue;
        }
        if (r.x < columnLeft) {
            continue;
        }
        int distY = (r.y >= dY) ? (r.y - dY) : (dY - r.y);
        if (distY < bestDistY || (distY == bestDistY && r.x < bestX)) {
            bestDistY = distY;
            bestX = r.x;
            startIdx = i;
        }
    }
    if (startIdx < 0) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }

    // PDF link destX is unreliable: poorly-authored links carry the source
    // page's body-text X, not the destination-page entry-start X. That lands
    // startIdx mid-line on hanging-indent description-list bibs, dropping
    // the leading "[KOS06]" / "Philippe Kruchten" portion from the popup.
    // Walk to the leftmost glyph of the line run containing startIdx so the
    // entry bounds always include the line's left edge. The gap-bounded run
    // keeps the walk within the destination's column in 2-column layouts —
    // the gutter is wider than spacing within a line — instead of latching
    // onto same-y text of another column. Also remember the run's right
    // edge: it estimates the column's right edge for the box passes below.
    int lineRunRightX;
    {
        int leftIdx = startIdx;
        int leftX = coords[startIdx].x;
        LineRunExtent(text, coords, startIdx, &leftIdx, &leftX, &lineRunRightX);
        // Only adopt the walked-left line start when startIdx didn't already
        // land on the entry's "[" label. The left walk exists for unreliable
        // PDF-link destX that lands mid-line; when startIdx is already the
        // bracket label, walking left can cross a narrow column gutter into a
        // neighbouring column whose row text reaches close to the gutter,
        // dragging the box into the wrong column.
        if (text.s[startIdx] != L'[') {
            startIdx = leftIdx;
        }
    }

    // Tight-y walk above can miss a "[VB25]"-style label that sits on a
    // slightly different baseline than its body line 1 (description-list
    // layouts where label and body use different fonts/sizes). If the
    // current leftmost still isn't a "[", search for one within roughly a
    // line height of destY at a smaller x — that's the bracket label of
    // the entry the link points at. Don't look further left than a hanging
    // indent (in 2-column layouts another column's "[" is much further).
    if (text.s[startIdx] != L'[') {
        constexpr int kMaxHangingIndentPt = 60;
        int sy = coords[startIdx].y;
        int sDy = coords[startIdx].dy;
        int yTol = sDy > 10 ? sDy : 10;
        int bracketIdx = -1;
        int bracketX = coords[startIdx].x;
        int minBracketX = coords[startIdx].x - kMaxHangingIndentPt;
        for (int i = 0; i < text.len; i++) {
            if (text.s[i] != L'[') {
                continue;
            }
            Rect r = coords[i];
            if (r.y < sy - yTol || r.y > sy + yTol) {
                continue;
            }
            if (r.x >= bracketX || r.x < minBracketX) {
                continue;
            }
            bracketX = r.x;
            bracketIdx = i;
        }
        if (bracketIdx >= 0) {
            startIdx = bracketIdx;
        }
    }

    // Right edge of the entry's column. Provisional value from the first
    // line's run; replaced below (once the entry's vertical extent is known) by
    // a gutter-aware scan of the page's column structure.
    int columnRightX = lineRunRightX + 40;
    // Left x of the entry body when it sits past a labelsep gap from the label
    // (hanging-indent "[TA05]  body"); -1 when label and body share one run.
    // The column scan starts here so it doesn't mistake the labelsep gap for a
    // gutter and clip the body.
    int entryBodyLeftX = -1;

    int firstLineLeftX = coords[startIdx].x;
    int firstLineY = coords[startIdx].y;
    int firstLineDy = coords[startIdx].dy;
    if (firstLineDy <= 0) {
        firstLineDy = 12;
    }

    // For a bracket label ("[N]" / "[Foo+09]"), the body starts just after the
    // closing "]". The gap between label and body (labelsep) is internal to the
    // entry but looks like a column gutter to the column scan below; start that
    // scan at the body so the labelsep isn't mistaken for a gutter (which would
    // clip the body to the label width).
    if (text.s[startIdx] == L'[') {
        int yTol = firstLineDy > 6 ? firstLineDy : 8;
        for (int i = startIdx + 1; i < text.len; i++) {
            if (abs(coords[i].y - firstLineY) > yTol) {
                continue;
            }
            if (text.s[i] == L']') {
                for (int j = i + 1; j < text.len; j++) {
                    WCHAR cj = text.s[j];
                    if (cj == L' ' || cj == L'\t' || cj == L'\n' || cj == L'\r') {
                        continue;
                    }
                    if (abs(coords[j].y - firstLineY) <= yTol && coords[j].x > coords[i].x) {
                        entryBodyLeftX = coords[j].x;
                    }
                    break;
                }
                break;
            }
        }
    }

    // Hanging-indent bracket entry: biblatex sizes the label column for the
    // widest label, so a narrow label ("[TA05]") is separated from its body
    // by a labelsep gap wider than LineRunExtent's within-line gap threshold.
    // The label-anchored run above then stops at the label, collapsing
    // lineRunRightX (and columnRightX) to the label width — that clips the
    // body horizontally and lets the box latch onto neighbouring labels.
    // Bridge the single labelsep gap: find the first glyph on the first line
    // right of the label run and extend a fresh run from there. The body run
    // is dense and stops at a real column gutter, so this stays 2-column-safe;
    // the bridge itself is capped at kMaxLabelSepPt, well under a gutter, so
    // it can't jump into an adjacent column.
    //
    // Only bridge when the first-line run is label-sized. When the label and
    // body share one line with no labelsep gap (e.g. "[2] M. Anvaari, …"),
    // LineRunExtent already spans the whole line and lineRunRightX sits at the
    // column's right edge — bridging from there would reach across a narrow
    // gutter (which can be < kMaxLabelSepPt) into the next column, blowing the
    // box width into the neighbouring entry.
    constexpr int kMaxLabelWidthPt = 70;
    if (text.s[startIdx] == L'[' && (lineRunRightX - firstLineLeftX) < kMaxLabelWidthPt) {
        constexpr int kMaxLabelSepPt = 50;
        int bandBot = firstLineY + (firstLineDy > 10 ? firstLineDy : 10);
        int bodyIdx = -1;
        int bodyX = INT_MAX;
        for (int i = 0; i < text.len; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.y < firstLineY - 3 || r.y > bandBot) {
                continue;
            }
            if (r.x <= lineRunRightX + 3 || r.x > lineRunRightX + kMaxLabelSepPt) {
                continue;
            }
            if (r.x < bodyX) {
                bodyX = r.x;
                bodyIdx = i;
            }
        }
        if (bodyIdx >= 0) {
            int bLeftIdx, bLeftX, bRightX;
            LineRunExtent(text, coords, bodyIdx, &bLeftIdx, &bLeftX, &bRightX);
            entryBodyLeftX = bLeftX;
            if (bRightX + 40 > columnRightX) {
                columnRightX = bRightX + 40;
            }
        }
    }

    // Entry line pitch (top-to-top of the first two lines): a stable measure of
    // inter-line spacing, unlike the tall "[" label glyph (firstLineDy). The
    // scan is bounded to roughly one column width right of the entry left so a
    // neighbouring column's line isn't mistaken for the next line.
    int linePitch = firstLineDy > 0 ? firstLineDy : 12;
    {
        constexpr int kColWidthMax = 250;
        int nextTop = INT_MAX;
        for (int i = 0; i < text.len; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.x < firstLineLeftX - 20 || r.x > firstLineLeftX + kColWidthMax) {
                continue;
            }
            if (r.y > firstLineY + 2 && r.y < nextTop) {
                nextTop = r.y;
            }
        }
        if (nextTop != INT_MAX) {
            int p = nextTop - firstLineY;
            if (p >= 4 && p < linePitch * 2) {
                linePitch = p;
            }
        }
    }

    // Gutter-bounded right edge of the entry's column, from the page's column
    // structure. Scan rightward from the body (after any label), marking
    // x-occupancy across a small band of lines around the entry, and stop at
    // the first vertical strip empty on every row (a real column gutter). A
    // long single line (URL) keeps its column occupied; a 2-column gutter is
    // empty across rows so the box never crosses into the next column. Computed
    // before the entry's vertical bounds so the trim/box below stay in-column.
    // The band is kept near the entry (not the whole page) so a centered page
    // number sitting in the gutter band lower down can't bridge the columns.
    {
        constexpr int kGutterW = 8;
        int bandTop = firstLineY - 2 * linePitch;
        int bandBot = firstLineY + 6 * linePitch;
        int scanStartX = (entryBodyLeftX >= 0) ? entryBodyLeftX : firstLineLeftX;
        int xLo = scanStartX - 5;
        if (xLo < 0) {
            xLo = 0;
        }
        int xHi = (int)mediabox.dx;
        if (xHi > xLo + 2) {
            int n = xHi - xLo;
            char* occ = AllocArray<char>(n);
            for (int i = 0; i < text.len; i++) {
                WCHAR c = text.s[i];
                if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                    continue;
                }
                Rect r = coords[i];
                if (r.y < bandTop || r.y > bandBot) {
                    continue;
                }
                int a = r.x - xLo;
                int b = r.x + r.dx - xLo;
                if (b <= 0 || a >= n) {
                    continue;
                }
                if (a < 0) {
                    a = 0;
                }
                if (b > n) {
                    b = n;
                }
                for (int x = a; x < b; x++) {
                    occ[x] = 1;
                }
            }
            int lastOcc = scanStartX;
            for (int x = scanStartX; x < xHi; x++) {
                int idx = x - xLo;
                if (idx >= 0 && idx < n && occ[idx]) {
                    lastOcc = x + 1;
                } else if (x - lastOcc >= kGutterW) {
                    break;
                }
            }
            free(occ);
            if (lastOcc > firstLineLeftX) {
                columnRightX = lastOcc;
            }
        }
    }

    // Bracket-style entry ("[ZM12]", "[1]", …): build the bounding box from
    // a y-range whose upper bound is the next "[" at firstLineLeftX. The
    // iterative scan below depends on text-array order, but some PDFs draw
    // labels and body in non-monotonic order — that made rule (a) terminate
    // on a *later* entry's "[" appearing early in the text array, before our
    // entry's body lines 2+. The y-range approach is order-independent.
    if (text.s[startIdx] == L'[') {
        int entryYBoundary = (int)mediabox.dy;
        // Set when a sibling "[" was actually found below this entry — as
        // opposed to entryYBoundary falling back to the page/cap bound because
        // this is the column's last entry (see foundSibling use below, which
        // feeds the column-wrap continuation search).
        bool foundSibling = false;
        for (int i = 0; i < text.len; i++) {
            if (i == startIdx) {
                continue;
            }
            if (text.s[i] != L'[') {
                continue;
            }
            Rect r = coords[i];
            // Accept "[" up to 30pt right of firstLineLeftX: some layouts
            // prefix entries with a page number or section index (e.g. a
            // "2" left of "[VB25]"), so the label "[" isn't exactly at
            // firstLineLeftX. Body-text "[…]" sits at indentX (≥ ~60pt
            // right of firstLineLeftX) so it's still excluded.
            if (r.x < firstLineLeftX - 5 || r.x > firstLineLeftX + 30) {
                continue;
            }
            // Half a line height below the first line's top is enough to be on a
            // *lower* line: the next entry's "[" sits a full line-pitch down.
            // Using the whole "[" glyph height fails when that height equals the
            // inter-line pitch (tall bracket glyph) — the next entry then lands
            // exactly at firstLineY+firstLineDy and is wrongly treated as the
            // same line, so the box swallows the following entry.
            if (r.y <= firstLineY + firstLineDy / 2) {
                continue;
            }
            if (r.y < entryYBoundary) {
                entryYBoundary = r.y;
                foundSibling = true;
            }
        }
        // Cap to a reasonable entry height so a last-on-page entry (no next
        // "[") doesn't sweep the page footer / page number into the popup.
        constexpr int kMaxBracketEntryPt = 250;
        int capY = firstLineY + kMaxBracketEntryPt;
        if (capY < entryYBoundary) {
            entryYBoundary = capY;
        }
        // Pull the boundary up by ~half a line height so the next entry's
        // first line — whose glyph tops can round to within 1–2 pt of the
        // "[" we picked — is reliably excluded.
        entryYBoundary -= 6;
        // Trailing-gap trim: a last-on-page entry has no sibling "[" below to
        // bound it, so entryYBoundary runs to the kMaxBracketEntryPt cap and
        // the box would swallow the page footer / page number (or a long
        // blank margin). Walk the entry's lines down from the first line and
        // stop at the first vertical gap wider than ~1.5 line heights — that
        // gap separates the entry from the footer. Inter-entry leading in a
        // dense bibliography is far smaller, so a real next entry (bounded by
        // its "[" above) is never trimmed: blockBottom keeps growing past
        // entryYBoundary and the trim is a no-op.
        {
            // Size the gap from the entry's line pitch (computed above), not
            // the tall "[" glyph height, so the trim doesn't over-reach across
            // a thin gap into a following block (e.g. a footnote past a rule).
            int lineH = linePitch;
            int gapThresh = lineH * 3 / 2;
            if (gapThresh < 12) {
                gapThresh = 12;
            }
            int prevBottom = firstLineY + lineH;
            int blockBottom = prevBottom;
            for (;;) {
                int nextBottom = -1;
                for (int i = 0; i < text.len; i++) {
                    WCHAR c = text.s[i];
                    if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                        continue;
                    }
                    Rect r = coords[i];
                    if (r.x < firstLineLeftX - 20 || r.x > columnRightX) {
                        continue;
                    }
                    // a glyph on a line strictly below the current block but
                    // within one gap of it (line tops cluster near the top y)
                    if (r.y <= prevBottom - 2 || r.y > prevBottom + gapThresh) {
                        continue;
                    }
                    if (r.y + r.dy > nextBottom) {
                        nextBottom = r.y + r.dy;
                    }
                }
                if (nextBottom < 0) {
                    break;
                }
                blockBottom = nextBottom;
                prevBottom = nextBottom;
            }
            if (blockBottom + 1 < entryYBoundary) {
                entryYBoundary = blockBottom + 1;
            }
        }
        int bMinX = INT_MAX, bMinY = INT_MAX, bMaxX = INT_MIN, bMaxY = INT_MIN;
        for (int i = 0; i < text.len; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[i];
            if (r.x < firstLineLeftX - 20 || r.x > columnRightX) {
                continue;
            }
            if (r.y < firstLineY - 5) {
                continue;
            }
            if (r.y >= entryYBoundary) {
                continue;
            }
            if (r.x < bMinX) {
                bMinX = r.x;
            }
            if (r.y < bMinY) {
                bMinY = r.y;
            }
            if (r.x + r.dx > bMaxX) {
                bMaxX = r.x + r.dx;
            }
            if (r.y + r.dy > bMaxY) {
                bMaxY = r.y + r.dy;
            }
        }
        if (bMinX != INT_MAX && (bMaxX - bMinX) >= 50 && (bMaxY - bMinY) >= 12) {
            RectF box{(float)bMinX - kEntryPadPt, (float)bMinY - kEntryPadPt,
                      (float)(bMaxX - bMinX) + 2.f * kEntryPadPt, (float)(bMaxY - bMinY) + 2.f * kEntryPadPt};
            ClipToMediabox(box, mediabox);
            if (box.dx >= 50.f && box.dy >= 20.f) {
                // No sibling "[" closed this entry, and — checked below — no
                // more text at all follows in this column: this looks like the
                // entry ran off the end of the column's content rather than
                // ending naturally (a real paragraph gap with more text after
                // it, which the trailing-gap trim above already accounts for).
                // Distance to the physical page bottom isn't a reliable signal
                // here: a column's last entry commonly sits well above the
                // page's bottom margin. Check whether it continues at the top
                // of the next column instead ("[63]"-style bibliography
                // entries wrapping across a 2-column page break).
                if (continuationOut && !foundSibling) {
                    // A page footer / page number often sits within the same
                    // x-range as the column, below the entry — that shouldn't
                    // block the wrap search. Distance alone doesn't separate it
                    // from a real continuation line (review-manuscript PDFs can
                    // have the footer just a line or two below the last
                    // entry), so instead measure each candidate line's full
                    // width: a page number is a short isolated token, while a
                    // real paragraph continuation line is (near-)column-width.
                    constexpr int kMinBodyLineWidthPt = 30;
                    bool moreBelowInColumn = false;
                    for (int i = 0; i < text.len && !moreBelowInColumn; i++) {
                        WCHAR c = text.s[i];
                        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                            continue;
                        }
                        Rect r = coords[i];
                        if (r.x < firstLineLeftX - 20 || r.x > columnRightX) {
                            continue;
                        }
                        if (r.y <= bMaxY) {
                            continue;
                        }
                        int leftIdx, leftX, rightX;
                        LineRunExtent(text, coords, i, &leftIdx, &leftX, &rightX);
                        // Confined to this column: a full-page-width footer
                        // credit line (journal name / affiliation, common
                        // below both columns) bridges clean across the gutter
                        // and would otherwise read as "real" wide body text.
                        bool confinedToColumn = rightX <= columnRightX + 10;
                        if (rightX - leftX >= kMinBodyLineWidthPt && confinedToColumn) {
                            moreBelowInColumn = true;
                        }
                    }
                    if (!moreBelowInColumn) {
                        *continuationOut = FindColumnWrapContinuation(text, coords, mediabox, columnRightX);
                    }
                }
                return box;
            }
        }
        // Fall through to the iterative-scan logic on degenerate result.
    }

    // 2. Scan forward to find the end of the entry.
    int endIdx = text.len;
    // Treat a glyph as "still on the current line" if its top y is above the
    // line's current max-bottom (with a small overlap tolerance). This is
    // robust to Word-style extraction quirks where glyphs on the same line
    // have varying y values (uppercase vs lowercase top, accent marks,
    // descenders) and may even be emitted in non-reading order.
    int currentLineY = firstLineY;
    int currentLineMaxBottom = firstLineY + firstLineDy;
    int prevBottom = firstLineY + firstLineDy;
    int lineHeight = firstLineDy;

    // Track leftmost X on the current line vs the previous line so we can
    // detect indent changes (the most reliable signal for author-year bibs).
    int currentLineLeftX = firstLineLeftX;
    int prevLineLeftX = INT_MAX;
    // X of the entry's continuation lines (captured from line 2). -1 = unknown.
    int indentX = -1;
    // Set when we observe another sibling entry start at firstLineLeftX with
    // no continuation indent in between — strong "this is a description list"
    // signal (e.g. "JVM Java Virtual Machine. 19, 36" / "LLM Large Language
    // Model. 45" abbreviation lists) that survives even when the current
    // entry is a single line.
    bool descListSibling = false;

    for (int i = startIdx + 1; i < text.len; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];

        // Stop on column wrap: y goes significantly above the current row.
        if (r.y < firstLineY - 5) {
            endIdx = i;
            break;
        }
        // Skip glyphs in other columns (left or right of the entry's column).
        if (r.x < firstLineLeftX - 20 || r.x > columnRightX) {
            continue;
        }

        // Only "major" glyphs (~letter-height) drive newline tracking.
        // Commas, periods, apostrophes, and other punctuation have small dy
        // and their r.y sits near the baseline, which can land at or past
        // currentLineMaxBottom and spuriously fire newline — corrupting the
        // measured line spacing.
        bool isMajorGlyph = (r.dy * 2 >= firstLineDy);
        bool isNewLine = isMajorGlyph && (r.y > currentLineMaxBottom - 2);
        if (isNewLine) {
            prevLineLeftX = currentLineLeftX;
            currentLineLeftX = r.x;
            // Promote currentLineMaxBottom (the line we're leaving) to
            // prevBottom *before* rule (c) checks, so the gap is measured
            // against the immediately-previous line — not the line two
            // transitions ago.
            prevBottom = currentLineMaxBottom;
        } else if (r.x < currentLineLeftX) {
            currentLineLeftX = r.x;
        }

        bool pastFirstLine = (r.y > firstLineY + firstLineDy * 3 / 4 + 2);
        bool atFirstLineLeftX = (r.x >= firstLineLeftX - 5 && r.x <= firstLineLeftX + 5);

        // Capture the continuation X from the entry's second line.
        if (isNewLine && pastFirstLine && indentX < 0 && !atFirstLineLeftX) {
            indentX = r.x;
        }

        // (a) "[" at the entry's first-line X = next entry marker. Works for
        // both numeric "[123]" and alphanumeric "[Foo+09]" / "[Bib05]" styles
        // — body-text "[…]" can't trigger this because body sits at indentX,
        // not firstLineLeftX.
        if (c == L'[' && atFirstLineLeftX) {
            descListSibling = true;
            endIdx = i;
            break;
        }

        // (b) Indent change: a new line back at the entry's first-line X
        // after a continuation line at a different X. Catches author-year
        // hanging-indent bibliographies where there's no [N] marker — this
        // is the primary signal for the *next* entry's start.
        if (isNewLine && atFirstLineLeftX && pastFirstLine && prevLineLeftX != INT_MAX &&
            (prevLineLeftX < firstLineLeftX - 5 || prevLineLeftX > firstLineLeftX + 5)) {
            descListSibling = true;
            endIdx = i;
            break;
        }

        // (c) Vertical paragraph break (no-indent style fallback). When the
        // glyph that triggered the gap is back at firstLineLeftX, the gap
        // is a blank line between description-list siblings (typical
        // abbreviation lists where each entry is separated by extra
        // vertical space) — treat as a sibling entry boundary. The
        // major-glyph newline tracking above keeps normal line spacing from
        // false-firing this rule, so it is safe to evaluate from line 1.
        if (r.y > prevBottom + lineHeight * 5 / 4) {
            if (atFirstLineLeftX) {
                descListSibling = true;
            }
            endIdx = i;
            break;
        }

        // (d) Single-line-entry case: a new line back at firstLineLeftX before
        // we discovered a continuation indent. The previous "entry" was one
        // line. Common pattern: stacked numbered footnotes "¹url\n²url\n³url"
        // or abbreviation lists ("JVM Java Virtual Machine. 19, 36").
        if (isNewLine && pastFirstLine && atFirstLineLeftX && indentX < 0 && prevLineLeftX != INT_MAX) {
            descListSibling = true;
            endIdx = i;
            break;
        }
        // (e) Line-count cap for author-year entries with no hanging indent —
        // common in Word-generated PDFs where continuation lines also start at
        // firstLineLeftX, so rule (d) would already have ended the entry. This
        // is a last-resort bound: most author-year bib entries fit in 5-6
        // lines, so cap at 6 to avoid bleeding into the following entry.
        WCHAR entryFirstC = text.s[startIdx];
        bool markedEntry = (entryFirstC == L'[' || entryFirstC == L'(' || (entryFirstC >= L'0' && entryFirstC <= L'9'));
        if (!markedEntry && isNewLine && indentX < 0) {
            int linesSinceStart = (lineHeight > 0) ? (r.y - firstLineY + lineHeight - 1) / lineHeight : 0;
            if (linesSinceStart >= 6) {
                endIdx = i;
                break;
            }
        }

        // Track current line height as we go (catches changing leading).
        if (isNewLine) {
            int dy = r.y - currentLineY;
            if (dy > 4 && dy < 60) {
                lineHeight = dy;
            }
            // prevBottom already promoted above (before rule checks).
            currentLineY = r.y;
            currentLineMaxBottom = r.y + r.dy;
        } else {
            // Only update line extents from major glyphs — punctuation
            // baselines would otherwise inflate max-bottom and shrink
            // currentLineY artificially.
            if (isMajorGlyph) {
                if (r.y < currentLineY) {
                    currentLineY = r.y;
                }
                if (r.y + r.dy > currentLineMaxBottom) {
                    currentLineMaxBottom = r.y + r.dy;
                }
            }
        }
    }

    // 3. Compute bounding box of glyphs in [startIdx, endIdx).
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (int i = startIdx; i < endIdx; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        Rect r = coords[i];
        // Exclude glyphs that aren't in the entry's column.
        if (r.x < firstLineLeftX - 20 || r.x > columnRightX) {
            continue;
        }
        if (r.y < firstLineY - 5) {
            continue;
        }
        if (r.x < minX) {
            minX = r.x;
        }
        if (r.y < minY) {
            minY = r.y;
        }
        if (r.x + r.dx > maxX) {
            maxX = r.x + r.dx;
        }
        if (r.y + r.dy > maxY) {
            maxY = r.y + r.dy;
        }
    }
    if (minX == INT_MAX) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }

    RectF box{(float)minX - kEntryPadPt, (float)minY - kEntryPadPt, (float)(maxX - minX) + 2.f * kEntryPadPt,
              (float)(maxY - minY) + 2.f * kEntryPadPt};
    ClipToMediabox(box, mediabox);
    if (box.dx < 50.f || box.dy < 20.f) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }
    // "Figure N.M" / "Table N.M" / "Listing N.M" / "Algorithm N.M" caption
    // anywhere below the detected box: the destination is a figure / table
    // / listing body. Override all other heuristics so the popup uses the
    // landscape view (caption included). Catches code/console listings
    // where each line happens to start with "[TAG]" — those would otherwise
    // be misclassified as description-list bibliography entries.
    {
        int boxBottomY = (int)(box.y + box.dy);
        for (int i = 0; i < text.len; i++) {
            if (coords[i].y <= boxBottomY) {
                continue;
            }
            if (IsCaptionLabelAt(text, i)) {
                // Let LandscapeBox handle the caption-extension — it has a
                // tighter, line-count-capped walk that doesn't sweep into
                // following body paragraphs.
                return LandscapeBox(mediabox, destX, destY, text, coords);
            }
        }
    }
    // Description-list bibliography ("[Smith2020]", "[1]", …) — unambiguous,
    // keep the fitted box.
    if (text.s[startIdx] == L'[') {
        return box;
    }
    // Tabular layout: continuation X far right of firstLineLeftX is a
    // column gap, not a hanging indent. Detection terminated at the first
    // data row; show the landscape view so the user sees the full table.
    if (indentX > 0 && (indentX - firstLineLeftX) > 80) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }
    // Section heading or caption-style label. Body paragraph below the
    // heading has first-line indent, so detection captures heading + body
    // line 1 and `indentX` lands in the same range as a hanging-indent bib.
    // Use the entry's first character / first word to disambiguate: real
    // bibliographies rarely start with a digit or with a label word like
    // "Figure"/"Table"/"Section". Catches "6.2 Foo", "Figure 2.2: …", etc.
    WCHAR firstC = text.s[startIdx];
    bool digitStart = (firstC >= L'0' && firstC <= L'9');
    bool labelStart = false;
    for (int off = 0; !labelStart && SeqStrAt(gHeadingPrefixWords, off);) {
        TempWStr w = ToWStrTemp(SeqStrAt(gHeadingPrefixWords, off));
        labelStart = MatchWordAt(text, startIdx, w, /*requireTrailingDigit=*/false);
        if (!SeqStrAdvance(gHeadingPrefixWords, off)) {
            break;
        }
    }
    if (digitStart || labelStart) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }
    // Code-listing detector: a high density of braces / semicolons / parens
    // within the detected box means the destination is most likely a code
    // listing presented as a figure. Bibliography prose almost never has
    // these characters at this density. Show the landscape view so the
    // popup also includes the figure caption below the code.
    {
        int codeChars = 0;
        int totalChars = 0;
        for (int i = startIdx; i < endIdx; i++) {
            WCHAR c = text.s[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            totalChars++;
            if (c == L'{' || c == L'}' || c == L';' || c == L'(' || c == L')') {
                codeChars++;
            }
        }
        if (totalChars > 50 && codeChars * 12 > totalChars) {
            return LandscapeBox(mediabox, destX, destY, text, coords);
        }
    }
    // Description-list / glossary / footnote-style entry: rule (a) or (d)
    // fired, meaning we saw a *sibling* entry start at firstLineLeftX. That
    // is a strong "this is a list of entries" signal even when the current
    // entry is a single line (abbreviations: "JVM Java Virtual Machine.").
    if (descListSibling) {
        return box;
    }
    // Single-line entry with no continuation indent and no sibling entry
    // detected — caption / heading / in-text cross-ref destination.
    if (box.dy < 30.f && indentX < 0) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }
    // Default: looks like a multi-line author-year bibliography entry,
    // keep the fitted box.
    return box;
}
