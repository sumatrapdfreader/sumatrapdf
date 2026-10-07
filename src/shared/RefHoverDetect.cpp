/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function popup-region detectors used by RefHover. Kept in a separate
// translation unit so the heuristics can be unit-tested with synthetic glyph
// arrays (see src/base/tests/RefHover_ut.cpp) without pulling in the engine,
// HWND, or rendering layers.

#include "base/Base.h"
#include "RefHover.h"

static constexpr float kAnchorTopMarginPt = 6.f;
// pt of padding around the detected entry box.
static constexpr float kEntryPadPt = 6.f;
static constexpr float kLatePageStartRatio = 0.70f;

bool ShouldSearchNextPage(RectF mediabox, float destY) {
    return mediabox.dy > 0.f && destY >= mediabox.dy * kLatePageStartRatio;
}

static bool IsGlyphSpace(WCHAR c) {
    return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r';
}

static bool IsAsciiAlnum(WCHAR c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
}

// Lowercase NFC words used in numbered captions and heading prefixes.
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

// Heading words in addition to the caption vocabulary.
static SeqStrings gHeadingWords =
    "section\0"
    "chapter\0"
    "abschnitt\0"
    "kapitel\0"
    "capítulo\0"
    "sección\0"
    "sezione\0"
    "capitolo\0"
    "seção\0"
    "secção\0"
    "chapitre\0";

enum class LabelKind {
    Caption,
    Heading
};

static bool MatchWordAt(WStr text, int idx, WStr w, LabelKind kind) {
    bool requireTrailingDigit = kind == LabelKind::Caption;
    int n = w.len;
    if (idx + n > text.len) {
        return false;
    }
    if (requireTrailingDigit && idx + n + 1 >= text.len) {
        return false;
    }
    if (wstr::CmpI(WStr(text.s + idx, n), w) != 0) {
        return false;
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

static bool MatchesLabelWords(WStr text, int idx, SeqStrings words, LabelKind kind) {
    for (Str word = SeqStrFirst(words); word; word = SeqStrNext(word)) {
        if (MatchWordAt(text, idx, ToWStrTemp(word), kind)) {
            return true;
        }
    }
    return false;
}

static bool IsCaptionLabelAt(WStr text, int idx) {
    if (idx > 0 && IsAsciiAlnum(text.s[idx - 1])) {
        return false;
    }
    return MatchesLabelWords(text, idx, gCaptionWords, LabelKind::Caption);
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

// Group glyphs by baseline so punctuation does not split visual lines.
// out needs glyphCount slots and must not alias coords.
void NormalizeGlyphLines(const Rect* coords, Rect* out, int glyphCount) {
    if (!coords || !out || glyphCount <= 0) {
        return;
    }
    constexpr int kBaselineTolPt = 4;
    constexpr int kMaxLines = 4096;
    struct Line {
        int baseline, top, bottom;
    };
    auto* lines = AllocArrayTemp<Line>(kMaxLines);
    int nLines = 0;
    for (int i = 0; i < glyphCount; i++) {
        int bl = coords[i].y + coords[i].dy;
        int best = -1;
        int bestDist = kBaselineTolPt + 1;
        for (int L = 0; L < nLines; L++) {
            int dist = bl - lines[L].baseline;
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
                lines[best] = {bl, coords[i].y, bl};
            } else {
                best = 0;
            }
        } else {
            lines[best].top = std::min(coords[i].y, lines[best].top);
            lines[best].bottom = std::max(bl, lines[best].bottom);
        }
        out[i] = coords[i];
        out[i].y = best; // Keep the line index until all bounds are known.
    }
    for (int i = 0; i < glyphCount; i++) {
        const Line& line = lines[out[i].y];
        out[i].y = line.top;
        out[i].dy = line.bottom - line.top;
    }
}

// Remove oversized glyphs on sparse baselines before NormalizeGlyphLines.
// Output arrays need glyphCount slots; returns the number kept.
int StripWatermarkGlyphs(WStr text, const Rect* coords, WCHAR* outText, Rect* outCoords) {
    int n = text.len;
    if (n <= 0 || !coords || !outText || !outCoords) {
        return 0;
    }
    // Typical body glyph height = the most common dy (the watermark, a heading,
    // and any super/subscripts are all minorities). Histogram over non-space
    // glyph heights and take the mode.
    constexpr int kMaxHistogramGlyphHeight = 4096;
    int maxDy = 0;
    for (int i = 0; i < n; i++) {
        if (coords[i].dy > maxDy && coords[i].dy <= kMaxHistogramGlyphHeight) {
            maxDy = coords[i].dy;
        }
    }
    int modeDy = 0;
    if (maxDy > 0) {
        int* hist = AllocArrayTemp<int>(maxDy + 1);
        if (hist) {
            for (int i = 0; i < n; i++) {
                if (IsGlyphSpace(text.s[i])) {
                    continue;
                }
                int d = coords[i].dy;
                if (d > 0 && d <= maxDy) {
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
        }
    }

    // Only strip when there's a stable body height to compare against, and only
    // glyphs clearly taller than it (1.5x) — well above tall "[" labels / caps.
    constexpr int kMinBodyDy = 4;
    bool canStrip = modeDy >= kMinBodyDy;
    int hgtThresh = modeDy + (modeDy / 2); // 1.5 * modeDy
    constexpr int kBaselineTolPt = 4;
    constexpr int kMinRowGlyphs = 3; // a real text row has at least this many

    int outLen = 0;
    for (int i = 0; i < n; i++) {
        WCHAR c = text.s[i];
        bool isSpace = IsGlyphSpace(c);
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
                if (IsGlyphSpace(text.s[j])) {
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

// Full-width strip from destY to the last glyph or caption block.
// Used when no entry or equation can be identified.
RectF LandscapeBox(RectF mediabox, float destX, float destY, WStr text, const Rect* coords) {
    (void)destX;
    float ty = (destY >= 0.f) ? destY - kAnchorTopMarginPt : 0.f;
    ty = std::max(ty, 0.f);
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
        newTy = std::max(newTy, 0.f);
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
    h = std::min(h, maxLandscape);
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
                pageRightX = std::max(rx, pageRightX);
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
                    capBot = prevLineBottom + (capLineH * 18 / 10);
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
                    lineTopY = std::min(gy, lineTopY);
                    int gb = gy + coords[j].dy;
                    lineBottomY = std::max(gb, lineBottomY);
                    int rx = coords[j].x + coords[j].dx;
                    lineRightX = std::max(rx, lineRightX);
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
            h = std::max(extendedH, h);
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
            if (IsGlyphSpace(text.s[i])) {
                continue;
            }
            Rect r = coords[i];
            if (r.y < boxTop || r.y >= boxBottom) {
                continue;
            }
            int glyphBottom = r.y + r.dy;
            lastTextBottom = std::max(glyphBottom, lastTextBottom);
        }
        float trimmedH = (float)lastTextBottom + kAnchorTopMarginPt - ty;
        if (trimmedH > 20.f && trimmedH < h) {
            h = trimmedH;
        }
    }
    return RectF{0.f, ty, mediabox.dx, h};
}

// Tight equation box for a right-aligned '(N)' or '(N.M)' label near destY.
// Returns empty when no equation label is found.
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
        int dist = abs(ly - dY);
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
    RectF box{0.f, (float)bestLabelY - pad, mediabox.dx, (float)bestLabelDy + (2.f * pad)};
    ClipToMediabox(box, mediabox);
    return box;
}

struct LineRun {
    int leftIdx;
    int leftX;
    int rightX;
};

// Expand a line run without crossing column gutters.
static LineRun LineRunExtent(WStr text, const Rect* coords, int anchorIdx) {
    constexpr int kMaxLineGapPt = 20;
    int sy = coords[anchorIdx].y;
    LineRun run{anchorIdx, coords[anchorIdx].x, coords[anchorIdx].x + coords[anchorIdx].dx};
    bool extended = true;
    while (extended) {
        extended = false;
        for (int i = 0; i < text.len; i++) {
            if (IsGlyphSpace(text.s[i])) {
                continue;
            }
            Rect r = coords[i];
            if (r.y < sy - 3 || r.y > sy + 3) {
                continue;
            }
            if (r.x < run.leftX && r.x + r.dx >= run.leftX - kMaxLineGapPt) {
                run.leftX = r.x;
                run.leftIdx = i;
                extended = true;
            }
            if (r.x + r.dx > run.rightX && r.x <= run.rightX + kMaxLineGapPt) {
                run.rightX = r.x + r.dx;
                extended = true;
            }
        }
    }
    return run;
}

static constexpr int kColumnGutterPt = 8;

// Stop at the first gutter empty across the selected band of text lines.
static int FindColumnRight(WStr text, const Rect* coords, int startX, int top, int bottom, int pageWidth) {
    int xLo = std::max(startX - 5, 0);
    if (pageWidth <= xLo + 2) {
        return INT_MIN;
    }
    int n = pageWidth - xLo;
    char* occ = AllocArrayTemp<char>(n);
    for (int i = 0; i < len(text); i++) {
        if (IsGlyphSpace(text.s[i])) {
            continue;
        }
        Rect r = coords[i];
        if (r.y < top || r.y > bottom) {
            continue;
        }
        int a = std::max(r.x - xLo, 0);
        int b = std::min(r.x + r.dx - xLo, n);
        for (int x = a; x < b; x++) {
            occ[x] = 1;
        }
    }
    int right = startX;
    for (int x = startX; x < pageWidth; x++) {
        int idx = x - xLo;
        if (idx >= 0 && idx < n && occ[idx]) {
            right = x + 1;
        } else if (x - right >= kColumnGutterPt) {
            break;
        }
    }
    return right;
}

// A bracket-style bibliography entry ("[63]") that runs to the bottom of its
// 2-column-layout column with no sibling "[" and no blank-line gap closing it
// may simply continue at the top of the next column (the column break falls
// mid-entry). Look for that continuation: a block of body text starting at
// the top of the column right of `oldColumnRightX` that does *not* itself
// begin with a "[" label (which would mean it's the next real entry, not a
// continuation). Returns an empty RectF when no such continuation is found.
static RectF FindColumnWrapContinuation(WStr text, const Rect* coords, RectF mediabox, int oldColumnRightX) {
    // 1. Left edge of the next column: leftmost glyph right of the old
    // column's right edge (skipping the gutter itself).
    int nextColLeftX = INT_MAX;
    for (int i = 0; i < text.len; i++) {
        if (IsGlyphSpace(text.s[i])) {
            continue;
        }
        Rect r = coords[i];
        if (r.x > oldColumnRightX + kColumnGutterPt && r.x < nextColLeftX) {
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
        if (IsGlyphSpace(text.s[i])) {
            continue;
        }
        Rect r = coords[i];
        if (r.x < nextColLeftX - 5 || r.y < kMinTopMarginPt) {
            continue;
        }
        LineRun run = LineRunExtent(text, coords, i);
        if (run.rightX - run.leftX > kColWidthMax || run.leftX < nextColLeftX - 20) {
            continue;
        }
        topY = std::min(r.y, topY);
    }
    if (topY == INT_MAX) {
        return RectF{};
    }
    int topLeftX = INT_MAX;
    int topDy = 12;
    for (int i = 0; i < text.len; i++) {
        if (IsGlyphSpace(text.s[i])) {
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

    int colRightX = nextColLeftX + 250;
    int right = FindColumnRight(text, coords, nextColLeftX, topY - 2, topY + (6 * topDy), (int)mediabox.dx);
    if (right > nextColLeftX) {
        colRightX = right;
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
        if (r.y <= topY + (topDy / 2) || r.y >= capY) {
            continue;
        }
        if (r.y < boundaryY) {
            boundaryY = r.y;
            closedBySibling = true;
        }
    }
    int bMinX = INT_MAX, bMinY = INT_MAX, bMaxX = INT_MIN, bMaxY = INT_MIN;
    for (int i = 0; i < text.len; i++) {
        if (IsGlyphSpace(text.s[i])) {
            continue;
        }
        Rect r = coords[i];
        if (r.x < nextColLeftX - 20 || r.x > colRightX) {
            continue;
        }
        // Without a closing sibling, a continuation must end within the cap.
        if (!closedBySibling && r.y >= capY - topDy) {
            return RectF{};
        }
        if (r.y < topY - 5 || r.y >= boundaryY) {
            continue;
        }
        bMinX = std::min(r.x, bMinX);
        bMinY = std::min(r.y, bMinY);
        bMaxX = std::max(r.x + r.dx, bMaxX);
        bMaxY = std::max(r.y + r.dy, bMaxY);
    }
    if (bMinX == INT_MAX || (bMaxX - bMinX) < 30 || (bMaxY - bMinY) < 8) {
        return RectF{};
    }
    RectF box{(float)bMinX - kEntryPadPt, (float)bMinY - kEntryPadPt, (float)(bMaxX - bMinX) + (2.f * kEntryPadPt),
              (float)(bMaxY - bMinY) + (2.f * kEntryPadPt)};
    ClipToMediabox(box, mediabox);
    return box;
}

// Fit a bibliography, glossary, or abbreviation entry; otherwise use LandscapeBox.
// continuationOut optionally receives the entry's tail in the next column.
RectF DetectEntryBox(WStr text, const Rect* coords, RectF mediabox, float destX, float destY, RectF* continuationOut) {
    if (continuationOut) {
        *continuationOut = RectF{};
    }
    // Show the whole page when sparse text would hide its images.
    constexpr int kSparsePageTextLen = 50;
    if (len(text) == 0 || text.len < kSparsePageTextLen || !coords) {
        return RectF{0.f, 0.f, mediabox.dx, mediabox.dy};
    }
    if (destY < 0.f) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }

    int dY = (int)destY;
    int dX = (int)destX;
    // Allow labels slightly left of destX while excluding columns further left.
    int columnLeft = (destX >= 0.f) ? dX - 15 : INT_MIN;

    // Pick the line nearest destY, then its leftmost glyph; a higher line may belong to another column.
    int startIdx = -1;
    int bestDistY = INT_MAX;
    int bestX = INT_MAX;
    for (int i = 0; i < text.len; i++) {
        if (IsGlyphSpace(text.s[i])) {
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
        return {};
    }

    // Links can point mid-line. Recover its left edge without crossing a column gutter.
    LineRun run = LineRunExtent(text, coords, startIdx);
    int lineRunRightX = run.rightX;
    // Keep an existing bracket label; walking left could cross a narrow gutter.
    if (text.s[startIdx] != L'[') {
        startIdx = run.leftIdx;
    }

    // Recover a bracket label whose baseline differs from the body, within one hanging indent.
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

    // Estimate the column edge from the line run until the gutter scan below.
    int columnRightX = lineRunRightX + 40;
    // Start the column scan past the label gap so it is not mistaken for a gutter.
    int entryBodyLeftX = -1;

    int firstLineLeftX = coords[startIdx].x;
    int firstLineY = coords[startIdx].y;
    int firstLineDy = coords[startIdx].dy;
    if (firstLineDy <= 0) {
        firstLineDy = 12;
    }

    // A bracket entry's body starts after "]"; its label gap is not a column gutter.
    if (text.s[startIdx] == L'[') {
        int yTol = firstLineDy > 6 ? firstLineDy : 8;
        for (int i = startIdx + 1; i < text.len; i++) {
            if (abs(coords[i].y - firstLineY) > yTol) {
                continue;
            }
            if (text.s[i] == L']') {
                for (int j = i + 1; j < text.len; j++) {
                    if (IsGlyphSpace(text.s[j])) {
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

    // Bridge the label gap only for a short label run; a full line could bridge into the next column.
    constexpr int kMaxLabelWidthPt = 70;
    if (text.s[startIdx] == L'[' && (lineRunRightX - firstLineLeftX) < kMaxLabelWidthPt) {
        constexpr int kMaxLabelSepPt = 50;
        int bandBot = firstLineY + (firstLineDy > 10 ? firstLineDy : 10);
        int bodyIdx = -1;
        int bodyX = INT_MAX;
        for (int i = 0; i < text.len; i++) {
            if (IsGlyphSpace(text.s[i])) {
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
            LineRun bodyRun = LineRunExtent(text, coords, bodyIdx);
            entryBodyLeftX = bodyRun.leftX;
            columnRightX = std::max(bodyRun.rightX + 40, columnRightX);
        }
    }

    // Measure line pitch within the column; tall bracket glyphs overestimate spacing.
    int linePitch = firstLineDy > 0 ? firstLineDy : 12;
    {
        constexpr int kColWidthMax = 250;
        int nextTop = INT_MAX;
        for (int i = 0; i < text.len; i++) {
            if (IsGlyphSpace(text.s[i])) {
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

    int scanStartX = entryBodyLeftX >= 0 ? entryBodyLeftX : firstLineLeftX;
    int right = FindColumnRight(text, coords, scanStartX, firstLineY - (2 * linePitch), firstLineY + (6 * linePitch),
                                (int)mediabox.dx);
    if (right > firstLineLeftX) {
        columnRightX = right;
    }

    // Bracket entries use the next label's y-coordinate because PDF text order can interleave entries.
    if (text.s[startIdx] == L'[') {
        int entryYBoundary = (int)mediabox.dy;
        // A final entry without a sibling may continue in the next column.
        bool foundSibling = false;
        for (int i = 0; i < text.len; i++) {
            if (i == startIdx) {
                continue;
            }
            if (text.s[i] != L'[') {
                continue;
            }
            Rect r = coords[i];
            // Allow a small prefix before the sibling label, excluding indented body-text brackets.
            if (r.x < firstLineLeftX - 5 || r.x > firstLineLeftX + 30) {
                continue;
            }
            // Half a glyph height distinguishes the next line even when a tall bracket spans the line pitch.
            if (r.y <= firstLineY + (firstLineDy / 2)) {
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
        entryYBoundary = std::min(capY, entryYBoundary);
        // Exclude the next entry's glyphs even when their tops differ slightly from its bracket.
        entryYBoundary -= 6;
        // Stop at a paragraph gap so the final entry excludes the footer.
        {
            // Use line pitch; bracket height can exceed line spacing.
            int lineH = linePitch;
            int gapThresh = lineH * 3 / 2;
            gapThresh = std::max(gapThresh, 12);
            int prevBottom = firstLineY + lineH;
            int blockBottom = prevBottom;
            for (;;) {
                int nextBottom = -1;
                for (int i = 0; i < text.len; i++) {
                    if (IsGlyphSpace(text.s[i])) {
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
                    nextBottom = std::max(r.y + r.dy, nextBottom);
                }
                if (nextBottom < 0) {
                    break;
                }
                blockBottom = nextBottom;
                prevBottom = nextBottom;
            }
            entryYBoundary = std::min(blockBottom + 1, entryYBoundary);
        }
        int bMinX = INT_MAX, bMinY = INT_MAX, bMaxX = INT_MIN, bMaxY = INT_MIN;
        for (int i = 0; i < text.len; i++) {
            if (IsGlyphSpace(text.s[i])) {
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
            bMinX = std::min(r.x, bMinX);
            bMinY = std::min(r.y, bMinY);
            bMaxX = std::max(r.x + r.dx, bMaxX);
            bMaxY = std::max(r.y + r.dy, bMaxY);
        }
        if (bMinX != INT_MAX && (bMaxX - bMinX) >= 50 && (bMaxY - bMinY) >= 12) {
            RectF box{(float)bMinX - kEntryPadPt, (float)bMinY - kEntryPadPt,
                      (float)(bMaxX - bMinX) + (2.f * kEntryPadPt), (float)(bMaxY - bMinY) + (2.f * kEntryPadPt)};
            ClipToMediabox(box, mediabox);
            if (box.dx >= 50.f && box.dy >= 20.f) {
                // Without a sibling or further body text, look for continuation in the next column.
                if (continuationOut && !foundSibling) {
                    // Short footer tokens do not count as following body text.
                    constexpr int kMinBodyLineWidthPt = 30;
                    bool moreBelowInColumn = false;
                    for (int i = 0; i < text.len && !moreBelowInColumn; i++) {
                        if (IsGlyphSpace(text.s[i])) {
                            continue;
                        }
                        Rect r = coords[i];
                        if (r.x < firstLineLeftX - 20 || r.x > columnRightX) {
                            continue;
                        }
                        if (r.y <= bMaxY) {
                            continue;
                        }
                        LineRun footerRun = LineRunExtent(text, coords, i);
                        // Exclude footer lines that span both columns.
                        bool confinedToColumn = footerRun.rightX <= columnRightX + 10;
                        if (footerRun.rightX - footerRun.leftX >= kMinBodyLineWidthPt && confinedToColumn) {
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
    // Track overlapping glyph bounds rather than identical tops; fonts and extraction order vary.
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
    // An aligned sibling with no hanging indent identifies a description list, even for single-line entries.
    bool descListSibling = false;

    for (int i = startIdx + 1; i < text.len; i++) {
        WCHAR c = text.s[i];
        if (IsGlyphSpace(c)) {
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

        // Small punctuation must not trigger a new line or distort its spacing.
        bool isMajorGlyph = (r.dy * 2 >= firstLineDy);
        bool isNewLine = isMajorGlyph && (r.y > currentLineMaxBottom - 2);
        if (isNewLine) {
            prevLineLeftX = currentLineLeftX;
            currentLineLeftX = r.x;
            // Rule (c) must measure the gap from the immediately preceding line.
            prevBottom = currentLineMaxBottom;
        } else if (r.x < currentLineLeftX) {
            currentLineLeftX = r.x;
        }

        bool pastFirstLine = (r.y > firstLineY + (firstLineDy * 3 / 4) + 2);
        bool atFirstLineLeftX = (r.x >= firstLineLeftX - 5 && r.x <= firstLineLeftX + 5);

        // Capture the continuation X from the entry's second line.
        if (isNewLine && pastFirstLine && indentX < 0 && !atFirstLineLeftX) {
            indentX = r.x;
        }

        // (a) An aligned bracket starts the next entry; body-text brackets are indented.
        if (c == L'[' && atFirstLineLeftX) {
            descListSibling = true;
            endIdx = i;
            break;
        }

        // (b) Returning from a hanging indent to the first-line x starts the next author-year entry.
        if (isNewLine && atFirstLineLeftX && pastFirstLine && prevLineLeftX != INT_MAX &&
            (prevLineLeftX < firstLineLeftX - 5 || prevLineLeftX > firstLineLeftX + 5)) {
            descListSibling = true;
            endIdx = i;
            break;
        }

        // (c) A paragraph gap ends the entry; an aligned next line identifies a sibling.
        if (r.y > prevBottom + (lineHeight * 5 / 4)) {
            if (atFirstLineLeftX) {
                descListSibling = true;
            }
            endIdx = i;
            break;
        }

        // (d) An aligned new line without a continuation indent identifies single-line siblings.
        if (isNewLine && pastFirstLine && atFirstLineLeftX && indentX < 0 && prevLineLeftX != INT_MAX) {
            descListSibling = true;
            endIdx = i;
            break;
        }
        // (e) Cap entries without a hanging indent at six lines to avoid swallowing the next entry.
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
            // Only major glyphs update line bounds; punctuation would inflate the baseline.
            if (isMajorGlyph) {
                currentLineY = std::min(r.y, currentLineY);
                currentLineMaxBottom = std::max(r.y + r.dy, currentLineMaxBottom);
            }
        }
    }

    // 3. Compute bounding box of glyphs in [startIdx, endIdx).
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (int i = startIdx; i < endIdx; i++) {
        if (IsGlyphSpace(text.s[i])) {
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
        minX = std::min(r.x, minX);
        minY = std::min(r.y, minY);
        maxX = std::max(r.x + r.dx, maxX);
        maxY = std::max(r.y + r.dy, maxY);
    }
    if (minX == INT_MAX) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }

    RectF box{(float)minX - kEntryPadPt, (float)minY - kEntryPadPt, (float)(maxX - minX) + (2.f * kEntryPadPt),
              (float)(maxY - minY) + (2.f * kEntryPadPt)};
    ClipToMediabox(box, mediabox);
    if (box.dx < 50.f || box.dy < 20.f) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }
    // A caption below the box identifies a figure or listing, even if its text resembles bracket entries.
    {
        int boxBottomY = (int)(box.y + box.dy);
        for (int i = 0; i < text.len; i++) {
            if (coords[i].y <= boxBottomY) {
                continue;
            }
            if (IsCaptionLabelAt(text, i)) {
                // LandscapeBox extends to the caption without swallowing following paragraphs.
                return LandscapeBox(mediabox, destX, destY, text, coords);
            }
        }
    }
    // Description-list bibliography ("[Smith2020]", "[1]", …) — unambiguous,
    // keep the fitted box.
    if (text.s[startIdx] == L'[') {
        return box;
    }
    // A wide continuation offset is a table column gap, not a hanging indent.
    if (indentX > 0 && (indentX - firstLineLeftX) > 80) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }
    // Numbers and heading labels distinguish sections or captions from author-year entries.
    WCHAR firstC = text.s[startIdx];
    bool digitStart = (firstC >= L'0' && firstC <= L'9');
    bool labelStart = MatchesLabelWords(text, startIdx, gCaptionWords, LabelKind::Heading) ||
                      MatchesLabelWords(text, startIdx, gHeadingWords, LabelKind::Heading);
    if (digitStart || labelStart) {
        return LandscapeBox(mediabox, destX, destY, text, coords);
    }
    // Dense code punctuation identifies a listing; include its caption in the landscape view.
    {
        int codeChars = 0;
        int totalChars = 0;
        for (int i = startIdx; i < endIdx; i++) {
            WCHAR c = text.s[i];
            if (IsGlyphSpace(c)) {
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
    // Keep the fitted box when an aligned sibling identifies a list or footnote entry.
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
