/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function popup-region detectors used by RefHover. Kept in a separate
// translation unit so the heuristics can be unit-tested with synthetic glyph
// arrays (see src/utils/tests/RefHover_ut.cpp) without pulling in the engine,
// HWND, or rendering layers.

#include "utils/BaseUtil.h"
#include "RefHoverDetect.h"

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
static bool MatchWordAt(const WCHAR* text, int textLen, int idx, const WCHAR* w, bool requireTrailingDigit) {
    int n = 0;
    while (w[n]) {
        n++;
    }
    if (idx + n > textLen) {
        return false;
    }
    if (requireTrailingDigit && idx + n + 1 >= textLen) {
        return false;
    }
    for (int j = 0; j < n; j++) {
        WCHAR c = FoldCaseW(text[idx + j]);
        if (c != w[j]) {
            return false;
        }
    }
    if (!requireTrailingDigit) {
        // require a trailing word boundary so that e.g. "Sections of ..."
        // doesn't match "section" or "Tableaux ..." match "tableau"
        if (idx + n < textLen) {
            WCHAR next = text[idx + n];
            if ((next >= L'a' && next <= L'z') || (next >= L'A' && next <= L'Z')) {
                return false;
            }
        }
        return true;
    }
    int k = idx + n;
    while (k < textLen && (text[k] == L' ' || text[k] == L'\t')) {
        k++;
    }
    return k < textLen && text[k] >= L'0' && text[k] <= L'9';
}

// True if `text[idx..]` starts a caption label like "Figure 1.2", "Tableau 2".
// See kCaptionWords for the language list. Requires the previous glyph to be
// a word boundary and the word to be followed by whitespace and a digit.
static bool IsCaptionLabelAt(const WCHAR* text, int textLen, int idx) {
    if (idx > 0 && IsAsciiAlnum(text[idx - 1])) {
        return false;
    }
    SeqStrings words = gCaptionWords;
    while (words) {
        TempWStr w = ToWStrTemp(words);
        if (MatchWordAt(text, textLen, idx, w, /*requireTrailingDigit=*/true)) {
            return true;
        }
        seqstrings::Next(words);
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

// Used when the link doesn't resolve to a recognizable bibliography entry —
// TOC targets, topbar/section links, table or figure captions, image-only
// PDFs. Returns a region that spans the full page width and goes from the
// destination Y down to the bottom of the last text glyph on the page —
// captures table caption / figure caption / section content below the
// link target without leaving a long blank margin at the popup bottom.
// Auto-fit in RefHoverOnTimer + the monitor-based popup height cap keep
// the popup a sensible size; the user can wheel-zoom in if text is too
// small.
RectF LandscapeBox(RectF mediabox, float destX, float destY, const WCHAR* text, const Rect* coords, int textLen) {
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
    if (text && coords && textLen > 0 && destY > 0.f) {
        int dY = (int)destY;
        for (int i = 0; i < textLen; i++) {
            int gy = coords[i].y;
            if (gy < dY - 5 || gy > dY + 15) {
                continue;
            }
            if (IsCaptionLabelAt(text, textLen, i)) {
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
    if (text && coords && textLen > 0) {
        // Search to end of page so tall figures with captions far below the
        // initial 200pt cap still match. The topmost (smallest y) "Figure
        // N.M" below the cap wins — PDFs draw text in arbitrary order, so
        // the first label in glyph-array order can be a caption much
        // further down the page.
        int searchTop = (int)(ty + h);
        int searchBot = (int)mediabox.dy;
        int capStartIdx = -1;
        int capBestY = INT_MAX;
        for (int i = 0; i < textLen; i++) {
            int gy = coords[i].y;
            if (gy < searchTop || gy > searchBot || gy >= capBestY) {
                continue;
            }
            if (IsCaptionLabelAt(text, textLen, i)) {
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
            for (int j = 0; j < textLen; j++) {
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
                for (int j = 0; j < textLen; j++) {
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
    if (text && coords && textLen > 0) {
        int boxTop = (int)ty;
        int boxBottom = (int)(ty + h);
        int lastTextBottom = boxTop;
        for (int i = 0; i < textLen; i++) {
            WCHAR c = text[i];
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
RectF DetectEquationBox(const WCHAR* text, const Rect* coords, int textLen, RectF mediabox, float destX, float destY) {
    (void)destX;
    RectF empty{};
    if (destY <= 0.f || !text || textLen <= 0 || !coords) {
        return empty;
    }
    int dY = (int)destY;

    // Scan glyphs in a band around destY. Find a ')' whose right edge is the
    // rightmost in its line, preceded by digits and an opening '('.
    int bestLabelY = -1;
    int bestLabelDy = 0;
    int bestDist = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        if (text[i] != L')') {
            continue;
        }
        int ly = coords[i].y;
        if (ly < dY - 40 || ly > dY + 40) {
            continue;
        }
        // Walk backward through digits on the same line.
        int p = i - 1;
        int digits = 0;
        while (p >= 0 && str::IsDigit(text[p]) && coords[p].y == ly) {
            p--;
            digits++;
        }
        if (digits == 0) {
            continue;
        }
        // Optional ".M" form.
        if (p >= 0 && text[p] == L'.' && coords[p].y == ly) {
            p--;
            int d2 = 0;
            while (p >= 0 && str::IsDigit(text[p]) && coords[p].y == ly) {
                p--;
                d2++;
            }
            if (d2 == 0) {
                continue;
            }
        }
        if (p < 0 || text[p] != L'(' || coords[p].y != ly) {
            continue;
        }
        int labelLeftX = coords[p].x;
        int labelRightX = coords[i].x + coords[i].dx;
        // Reject if any non-space glyph on the same line sits further right
        // than the label — equation labels are line-trailing by construction.
        bool hasRightOf = false;
        for (int j = 0; j < textLen; j++) {
            if (j >= p && j <= i) {
                continue;
            }
            if (str::IsWs(text[j])) {
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
static void LineRunExtent(const WCHAR* text, const Rect* coords, int textLen, int anchorIdx, int* leftIdxOut,
                          int* leftXOut, int* rightXOut) {
    constexpr int kMaxLineGapPt = 20;
    int sy = coords[anchorIdx].y;
    int leftIdx = anchorIdx;
    int leftX = coords[anchorIdx].x;
    int rightX = coords[anchorIdx].x + coords[anchorIdx].dx;
    bool extended = true;
    while (extended) {
        extended = false;
        for (int i = 0; i < textLen; i++) {
            WCHAR c = text[i];
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
RectF DetectEntryBox(const WCHAR* text, const Rect* coords, int textLen, RectF mediabox, float destX, float destY) {
    // Sparse-text dest page (image-only or near-image-only — e.g. a
    // children's PDF overview with character thumbnails plus a single
    // heading). Fitting to the heading line gives a thin sliver and hides
    // the actual content. Show the whole page so the user sees what they
    // would navigate to; the auto-fit in RefHoverOnTimer scales the bitmap
    // to popup limits.
    constexpr int kSparsePageTextLen = 50;
    if (!text || textLen < kSparsePageTextLen || !coords) {
        return RectF{0.f, 0.f, mediabox.dx, mediabox.dy};
    }
    if (destY < 0.f) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }

    int dY = (int)destY;
    int dX = (int)destX;
    // Constrain to the destination's column — for 2-column layouts this
    // prevents the search from latching onto same-Y body text in another
    // column. We allow a small left tolerance so a "[1]" whose [ starts
    // a few pt left of destX still matches.
    int columnLeft = (destX >= 0.f) ? dX - 15 : INT_MIN;

    // 1. Find the start glyph: top-left non-whitespace glyph with
    //    y in [destY-5, destY+30] and x at-or-right-of columnLeft.
    int startIdx = -1;
    int bestY = INT_MAX;
    int bestX = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        WCHAR c = text[i];
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
        if (r.y < bestY || (r.y == bestY && r.x < bestX)) {
            bestY = r.y;
            bestX = r.x;
            startIdx = i;
        }
    }
    if (startIdx < 0) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
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
        LineRunExtent(text, coords, textLen, startIdx, &leftIdx, &leftX, &lineRunRightX);
        startIdx = leftIdx;
    }

    // Tight-y walk above can miss a "[VB25]"-style label that sits on a
    // slightly different baseline than its body line 1 (description-list
    // layouts where label and body use different fonts/sizes). If the
    // current leftmost still isn't a "[", search for one within roughly a
    // line height of destY at a smaller x — that's the bracket label of
    // the entry the link points at. Don't look further left than a hanging
    // indent (in 2-column layouts another column's "[" is much further).
    if (text[startIdx] != L'[') {
        constexpr int kMaxHangingIndentPt = 60;
        int sy = coords[startIdx].y;
        int sDy = coords[startIdx].dy;
        int yTol = sDy > 10 ? sDy : 10;
        int bracketIdx = -1;
        int bracketX = coords[startIdx].x;
        int minBracketX = coords[startIdx].x - kMaxHangingIndentPt;
        for (int i = 0; i < textLen; i++) {
            if (text[i] != L'[') {
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

    // glyphs right of this aren't part of the entry's column (2-column
    // layouts); the slack covers ragged-right lines longer than line 1
    int columnRightX = lineRunRightX + 40;

    int firstLineLeftX = coords[startIdx].x;
    int firstLineY = coords[startIdx].y;
    int firstLineDy = coords[startIdx].dy;
    if (firstLineDy <= 0) {
        firstLineDy = 12;
    }

    // Bracket-style entry ("[ZM12]", "[1]", …): build the bounding box from
    // a y-range whose upper bound is the next "[" at firstLineLeftX. The
    // iterative scan below depends on text-array order, but some PDFs draw
    // labels and body in non-monotonic order — that made rule (a) terminate
    // on a *later* entry's "[" appearing early in the text array, before our
    // entry's body lines 2+. The y-range approach is order-independent.
    if (text[startIdx] == L'[') {
        int entryYBoundary = (int)mediabox.dy;
        for (int i = 0; i < textLen; i++) {
            if (i == startIdx) {
                continue;
            }
            if (text[i] != L'[') {
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
            if (r.y <= firstLineY + firstLineDy) {
                continue;
            }
            if (r.y < entryYBoundary) {
                entryYBoundary = r.y;
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
        int bMinX = INT_MAX, bMinY = INT_MAX, bMaxX = INT_MIN, bMaxY = INT_MIN;
        for (int i = 0; i < textLen; i++) {
            WCHAR c = text[i];
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
                return box;
            }
        }
        // Fall through to the iterative-scan logic on degenerate result.
    }

    // 2. Scan forward to find the end of the entry.
    int endIdx = textLen;
    int prevY = firstLineY;
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

    for (int i = startIdx + 1; i < textLen; i++) {
        WCHAR c = text[i];
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

        bool isNewLine = (r.y > prevY + 2);
        if (isNewLine) {
            prevLineLeftX = currentLineLeftX;
            currentLineLeftX = r.x;
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
        // vertical space) — treat as a sibling entry boundary.
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

        // Track current line height as we go (catches changing leading).
        if (isNewLine) {
            int dy = r.y - prevY;
            if (dy > 4 && dy < 60) {
                lineHeight = dy;
            }
            prevY = r.y;
            prevBottom = r.y + r.dy;
        }
    }

    // 3. Compute bounding box of glyphs in [startIdx, endIdx).
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (int i = startIdx; i < endIdx; i++) {
        WCHAR c = text[i];
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
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }

    RectF box{(float)minX - kEntryPadPt, (float)minY - kEntryPadPt, (float)(maxX - minX) + 2.f * kEntryPadPt,
              (float)(maxY - minY) + 2.f * kEntryPadPt};
    ClipToMediabox(box, mediabox);
    if (box.dx < 50.f || box.dy < 20.f) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    // "Figure N.M" / "Table N.M" / "Listing N.M" / "Algorithm N.M" caption
    // anywhere below the detected box: the destination is a figure / table
    // / listing body. Override all other heuristics so the popup uses the
    // landscape view (caption included). Catches code/console listings
    // where each line happens to start with "[TAG]" — those would otherwise
    // be misclassified as description-list bibliography entries.
    {
        int boxBottomY = (int)(box.y + box.dy);
        for (int i = 0; i < textLen; i++) {
            if (coords[i].y <= boxBottomY) {
                continue;
            }
            if (IsCaptionLabelAt(text, textLen, i)) {
                // Let LandscapeBox handle the caption-extension — it has a
                // tighter, line-count-capped walk that doesn't sweep into
                // following body paragraphs.
                return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
            }
        }
    }
    // Description-list bibliography ("[Smith2020]", "[1]", …) — unambiguous,
    // keep the fitted box.
    if (text[startIdx] == L'[') {
        return box;
    }
    // Tabular layout: continuation X far right of firstLineLeftX is a
    // column gap, not a hanging indent. Detection terminated at the first
    // data row; show the landscape view so the user sees the full table.
    if (indentX > 0 && (indentX - firstLineLeftX) > 80) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    // Section heading or caption-style label. Body paragraph below the
    // heading has first-line indent, so detection captures heading + body
    // line 1 and `indentX` lands in the same range as a hanging-indent bib.
    // Use the entry's first character / first word to disambiguate: real
    // bibliographies rarely start with a digit or with a label word like
    // "Figure"/"Table"/"Section". Catches "6.2 Foo", "Figure 2.2: …", etc.
    WCHAR firstC = text[startIdx];
    bool digitStart = (firstC >= L'0' && firstC <= L'9');
    bool labelStart = false;
    SeqStrings words = gHeadingPrefixWords;
    while (!labelStart && words) {
        TempWStr w = ToWStrTemp(words);
        labelStart = MatchWordAt(text, textLen, startIdx, w, /*requireTrailingDigit=*/false);
        seqstrings::Next(words);
    }
    if (digitStart || labelStart) {
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
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
            WCHAR c = text[i];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            totalChars++;
            if (c == L'{' || c == L'}' || c == L';' || c == L'(' || c == L')') {
                codeChars++;
            }
        }
        if (totalChars > 50 && codeChars * 12 > totalChars) {
            return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
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
        return LandscapeBox(mediabox, destX, destY, text, coords, textLen);
    }
    // Default: looks like a multi-line author-year bibliography entry,
    // keep the fitted box.
    return box;
}
