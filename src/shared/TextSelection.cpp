/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "DocController.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "TextSelection.h"

uint distSq(int x, int y) {
    return (x * x) + (y * y);
}
// underscore is mainly used for programming and is thus considered a word character
bool isWordChar(int c) {
#if OS_WIN
    return (c > 0 && c <= 0xffff && IsCharAlphaNumericW((WCHAR)c)) || c == '_';
#else
    return (c > 0 && c <= 0xffff && iswalnum((wint_t)c)) || c == '_';
#endif
}

static bool isDigit(int c) {
    return c >= '0' && c <= '9';
}

TextSelection::TextSelection(EngineBase* engine) : engine(engine) {}

void TextSelection::Reset() {
    VecReset(result);
    wordStartPage = wordStartGlyph = wordEndPage = wordEndGlyph = -1;
}

static bool GlyphContains(Rect coord, const QuadF* quads, int i, PointF pt, Point pti) {
    if (quads && quads[i].IsRotated()) {
        return quads[i].Contains(pt);
    }
    return coord.Contains(pti);
}

// returns the index of the glyph closest to the right of the given coordinates
// (i.e. when over the right half of a glyph, the returned index will be for the
// glyph following it, which will be the first glyph (not) to be selected)
// engine is only consulted for upright glyphs
int FindClosestGlyphIn(EngineBase* engine, int pageNo, Rect* coords, QuadF* quads, int textLen, double x, double y) {
    PointF pt = PointF((float)x, (float)y);

    unsigned int maxDist = UINT_MAX;
    Point pti = ToPoint(pt);
    bool overGlyph = false;
    int result = -1;

    for (int i = 0; i < textLen; i++) {
        Rect& coord = coords[i];
        if (!coord.x && !coord.dx) {
            continue;
        }
        bool inside = GlyphContains(coord, quads, i, pt, pti);
        if (overGlyph && !inside) {
            continue;
        }

        int cx, cy;
        if (quads && quads[i].IsRotated()) {
            PointF c = quads[i].Center();
            cx = (int)c.x;
            cy = (int)c.y;
        } else {
            cx = coord.x + (coord.dx / 2);
            cy = coord.y + (coord.dy / 2);
        }
        uint dist = distSq((int)x - cx, (int)y - cy);
        // Prefer containing glyphs, then the nearest center.
        if (dist < maxDist || (!overGlyph && inside)) {
            result = i;
            maxDist = dist;
        }
        overGlyph = overGlyph || inside;
    }

    if (-1 == result) {
        return 0;
    }
    ReportIf(result < 0 || result >= textLen);

    // the result indexes the first glyph to be selected in a forward selection.
    // Along the baseline for rotated glyphs; along +x for upright ones.
    bool pastMid = false;
    if (quads && quads[result].IsRotated()) {
        PointF ul = quads[result].ul;
        PointF ur = quads[result].ur;
        float dx = ur.x - ul.x;
        float dy = ur.y - ul.y;
        float den = (dx * dx) + (dy * dy);
        if (den > 0.01f) {
            float t = (((pt.x - ul.x) * dx) + ((pt.y - ul.y) * dy)) / den;
            float ax = ul.x + (t * dx);
            float ay = ul.y + (t * dy);
            float perp2 = ((pt.x - ax) * (pt.x - ax)) + ((pt.y - ay) * (pt.y - ay));
            float hx = quads[result].ll.x - ul.x;
            float hy = quads[result].ll.y - ul.y;
            float height2 = (hx * hx) + (hy * hy);
            // ignore the half-glyph split when the point is far off the baseline
            // (e.g. F7 caret at the page's top-left)
            if (overGlyph || perp2 <= height2 * 4.f) {
                pastMid = t > 0.5f;
            }
        }
    } else {
        RectF bbox = engine->Transform(ToRectF(coords[result]), pageNo, 1.0, 0);
        PointF ptT = engine->Transform(pt, pageNo, 1.0, 0);
        pastMid = ptT.x > bbox.x + (0.5 * bbox.dx);
    }
    // for some (DjVu) documents, all glyphs of a word share the same bbox.
    // Rotated glyphs are told apart by their quads: small rotated text can
    // round distinct glyphs to the same int bbox.
    auto sharesBox = [&](int i) -> bool {
        if (quads && (quads[i].IsRotated() || quads[i - 1].IsRotated())) {
            return false;
        }
        return coords[i] == coords[i - 1];
    };
    if (pastMid) {
        result++;
        while (result < textLen && sharesBox(result)) {
            result++;
        }
    }
    ReportIf(result > 0 && result < textLen && sharesBox(result));

    return result;
}

int TextSelection::FindClosestGlyphAt(int pageNo, double x, double y) {
    Rect* coords;
    QuadF* quads = nullptr;
    int textLen = 0;
    engine->GetTextForPage(pageNo, &textLen, &coords, &quads);
    return FindClosestGlyphIn(engine, pageNo, coords, quads, textLen, x, y);
}

// Dehyphenation removes both the trailing hyphen and the line-separator glyph,
// so adjacent coords can belong to different visual lines. Require at least
// half of the new glyph's height to overlap the current line box; this still
// keeps smaller subscript and superscript glyphs in the same run.
static bool IsGlyphOnVisualLine(Rect lineBox, Rect glyphBox) {
    int top = lineBox.y > glyphBox.y ? lineBox.y : glyphBox.y;
    int bottom = lineBox.y + lineBox.dy < glyphBox.y + glyphBox.dy ? lineBox.y + lineBox.dy : glyphBox.y + glyphBox.dy;
    return (bottom - top) * 2 >= glyphBox.dy;
}

void FillSelectionRects(Vec<TextSel>* result, int pageNo, Rect* coords, int textLen, int glyph, int length,
                        Rect mediabox, QuadF* glyphQuads) {
    Rect *c = &coords[glyph], *end = c + length;
    while (c < end) {
        // skip line breaks (empty boxes: hard newlines and soft-join spaces)
        for (; c < end && !c->x && !c->dx; c++) {
            // no-op
        }
        if (c >= end) {
            break;
        }

        int runStart = (int)(c - coords);
        bool rotated = glyphQuads && glyphQuads[runStart].IsRotated();
        if (rotated) {
            for (; c < end && (c->x || c->dx); c++) {
                int ix = (int)(c - coords);
                if (ix > runStart && !IsGlyphOnVisualLine(coords[runStart], *c)) {
                    break;
                }
                Rect bbox = c->Intersect(mediabox);
                if (bbox.IsEmpty()) {
                    continue;
                }
                VecAppend(*result, TextSel{pageNo, bbox, glyphQuads[ix]});
            }
            continue;
        }

        Rect bbox;
        for (; c < end && (c->x || c->dx); c++) {
            if (!bbox.IsEmpty() && !IsGlyphOnVisualLine(bbox, *c)) {
                break;
            }
            bbox = bbox.Union(*c);
        }
        bbox = bbox.Intersect(mediabox);
        // skip text that's completely outside a page's mediabox
        if (bbox.IsEmpty()) {
            continue;
        }

        // Only clip against the next glyph when it is on this visual line.
        // At a dehyphenated break the next glyph belongs to the following line.
        bool overlapsVertically = c < coords + textLen && c->y < bbox.y + bbox.dy && c->y + c->dy > bbox.y;
        if (overlapsVertically && (c->x || c->dx) && bbox.x < c->x && bbox.x + bbox.dx > c->x) {
            bbox.dx = c->x - bbox.x;
        }

        VecAppend(*result, TextSel{pageNo, bbox, {}});
    }
}

// A visual line of page text: glyph range plus the box of its first printable
// glyph. mono: every printable glyph has the same advance, i.e. a code listing,
// where indentation and blank lines carry meaning.
struct PageLine {
    int start = 0;
    int end = 0; // [start, end), excludes the newline glyph
    int x0 = 0;
    int y0 = 0;
    int h = 0;
    float glyphW = 0;
    int nGlyphs = 0; // printable glyphs
    bool hasContent = false;
    bool mono = false;
    bool code = false;
};

// Glyph boxes are page units rounded to int, so a monospace advance of 5.4
// shows up as 5 and 6.
constexpr int kMonoWidthTolerance = 1;
constexpr int kMaxBlankLines = 2;
// Digits share one width in most fonts and one glyph always does, so a lone
// page number looks monospace. A block of mono lines is code only when one
// of them has this many equal-width glyphs; short lines like "}" follow it.
constexpr int kMinCodeGlyphs = 6;

// Flags runs of consecutive monospace lines as code when one of them is long
// enough to prove the font is monospace (see kMinCodeGlyphs).
static void MarkCodeLines(Vec<PageLine>& lines) {
    int n = len(lines);
    int i = 0;
    while (i < n) {
        if (!lines[i].hasContent || !lines[i].mono) {
            i++;
            continue;
        }
        int runEnd = i;
        bool isCode = false;
        while (runEnd < n && lines[runEnd].hasContent && lines[runEnd].mono) {
            isCode = isCode || lines[runEnd].nGlyphs >= kMinCodeGlyphs;
            runEnd++;
        }
        for (int j = i; j < runEnd; j++) {
            lines[j].code = isCode;
        }
        i = runEnd;
    }
}

// Splits page text into visual lines at the empty-box newline glyphs.
static void CollectPageLines(Str text, Rect* coords, int textLen, Vec<PageLine>& lines) {
    PageLine cur;
    int minW = 0;
    int maxW = 0;
    int nW = 0;
    float sumW = 0;
    auto finish = [&](int end) {
        cur.end = end;
        if (nW > 0) {
            cur.glyphW = sumW / (float)nW;
            cur.nGlyphs = nW;
            cur.mono = (maxW - minW) <= kMonoWidthTolerance;
        }
        VecAppend(lines, cur);
        cur = PageLine{};
        cur.start = end + 1;
        minW = maxW = nW = 0;
        sumW = 0;
    };

    int byteIdx = 0;
    for (int i = 0; i < textLen; i++) {
        int cp = Utf8CodepointNext(text, byteIdx);
        Rect& r = coords[i];
        if (!r.x && !r.dx) {
            // newline or soft-join space from FzTextPageToUtf8
            if (cp == '\n' || cp == '\r') {
                finish(i);
            }
            continue;
        }
        if (cp == ' ' || cp == '\t') {
            continue;
        }
        if (!cur.hasContent) {
            cur.hasContent = true;
            cur.x0 = r.x;
            cur.y0 = r.y;
            cur.h = r.dy;
        }
        minW = nW == 0 ? r.dx : std::min(minW, r.dx);
        maxW = std::max(maxW, r.dx);
        sumW += (float)r.dx;
        nW++;
    }
    if (cur.start < textLen) {
        finish(textLen);
    }
    MarkCodeLines(lines);
}

// Smallest vertical step between consecutive lines on the page. Glyph box
// height is no substitute: Courier's box is ~0.8em but its pitch ~1.2em.
static int PageLinePitch(const Vec<PageLine>& lines, int minPitch) {
    int pitch = 0;
    const PageLine* prev = nullptr;
    for (const PageLine& l : lines) {
        if (!l.hasContent) {
            continue;
        }
        if (prev) {
            int d = l.y0 - prev->y0;
            if (d >= minPitch && (pitch == 0 || d < pitch)) {
                pitch = d;
            }
        }
        prev = &l;
    }
    return pitch;
}

// Text of the selected glyph range, one entry per visual line. With layout,
// code lines get their indentation back as spaces, measured from the leftmost
// selected code line, and the vertical gaps between them become blank lines
// (#2423). Prose is left alone: centered titles and first-line indents would
// otherwise sprout leading spaces.
static void AppendSelectedLines(Str text, Rect* coords, int textLen, int glyph, int length, StrVec* out, bool layout) {
    Vec<PageLine> lines;
    CollectPageLines(text, coords, textLen, lines);
    int selEnd = glyph + length;
    auto selected = [&](const PageLine& l) { return l.end > glyph && l.start < selEnd; };

    int refX0 = -1;
    if (layout) {
        for (const PageLine& l : lines) {
            if (selected(l) && l.code && (refX0 < 0 || l.x0 < refX0)) {
                refX0 = l.x0;
            }
        }
    }

    const PageLine* prev = nullptr;
    for (const PageLine& l : lines) {
        if (!selected(l)) {
            continue;
        }
        int from = std::max(l.start, glyph);
        int to = std::min(l.end, selEnd);
        str::Builder b;
        if (layout && l.code) {
            if (prev && prev->code) {
                int gap = l.y0 - prev->y0;
                int pitch = PageLinePitch(lines, l.h / 2);
                int nBlank = (gap > 0 && pitch > 0) ? ((gap + (pitch / 2)) / pitch) - 1 : 0;
                nBlank = std::clamp(nBlank, 0, kMaxBlankLines);
                for (int i = 0; i < nBlank; i++) {
                    out->Append(StrL(""));
                }
            }
            // a selection starting mid-line has no column to restore
            if (from == l.start && l.glyphW > 0) {
                int nSpaces = (int)roundf((float)(l.x0 - refX0) / l.glyphW);
                for (int i = 0; i < nSpaces; i++) {
                    b.AppendChar(' ');
                }
            }
        }
        if (to > from) {
            b.Append(Utf8SliceByCodepoints(text, from, to - from));
        }
        if (len(b) > 0) {
            out->Append(ToStr(b));
        }
        if (l.hasContent) {
            prev = &l;
        }
    }
}

static void FillResultRects(TextSelection* ts, int pageNo, int glyph, int length, StrVec* lines = nullptr,
                            bool layout = false) {
    Rect* coords;
    QuadF* quads = nullptr;
    int textLen = 0;
    Str text = ts->engine->GetTextForPage(pageNo, &textLen, &coords, &quads);
    // Clamp ranges that outlive their page text (stale find-match coords after
    // tab close/reload, or a multi-page match that ends past a shorter page).
    if (glyph < 0) {
        length += glyph;
        glyph = 0;
    }
    length = std::max(length, 0);
    glyph = std::min(glyph, textLen);
    if (glyph + length > textLen) {
        length = textLen - glyph;
    }
    if (length <= 0 || !coords) {
        return;
    }
    Rect mediabox = ts->engine->PageMediabox(pageNo).Round();

    // Copy soft-join spaces inside a line; only empty-box newlines split it.
    // Highlighting below still splits at every empty box.
    if (lines) {
        AppendSelectedLines(text, coords, textLen, glyph, length, lines, layout);
        return;
    }

    FillSelectionRects(&ts->result, pageNo, coords, textLen, glyph, length, mediabox, quads);
}

bool TextSelection::IsOverGlyph(int pageNo, double x, double y) {
    Rect* coords;
    QuadF* quads = nullptr;
    int textLen = 0;
    if (!engine->TryGetTextForPage(pageNo, &textLen, &coords, &quads)) {
        return false;
    }

    int glyphIx = FindClosestGlyphIn(engine, pageNo, coords, quads, textLen, x, y);
    PointF ptf((float)x, (float)y);
    Point pt = ToPoint(ptf);
    auto contains = [&](int i) -> bool {
        if (i < 0 || i >= textLen) {
            return false;
        }
        return GlyphContains(coords[i], quads, i, ptf, pt);
    };
    // when over the right half of a glyph, FindClosestGlyphIn returns the
    // index of the next glyph, in which case glyphIx must be decremented
    return contains(glyphIx) || contains(glyphIx - 1);
}

void TextSelection::StartAt(int pageNo, int glyphIx) {
    startPage = pageNo;
    startGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen = 0;
        engine->GetTextForPage(pageNo, &textLen);
        startGlyph += textLen + 1;
    }
}

void TextSelection::StartAt(int pageNo, double x, double y) {
    StartAt(pageNo, FindClosestGlyphAt(pageNo, x, y));
}

void TextSelection::SelectUpTo(int pageNo, double x, double y) {
    SelectUpTo(pageNo, FindClosestGlyphAt(pageNo, x, y));
}

void TextSelection::SelectUpTo(int pageNo, int glyphIx) {
    if (startPage == -1 || startGlyph == -1) {
        return;
    }

    endPage = pageNo;
    endGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen = 0;
        engine->GetTextForPage(pageNo, &textLen);
        endGlyph = textLen + glyphIx + 1;
    }

    VecClear(result);
    int fromPage, fromGlyph, toPage, toGlyph;
    GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);

    for (int page = fromPage; page <= toPage; page++) {
        int textLen = 0;
        engine->GetTextForPage(page, &textLen);

        int glyph = page == fromPage ? fromGlyph : 0;
        int end = page == toPage ? toGlyph : textLen;
        glyph = std::max(glyph, 0);
        end = std::min(end, textLen);
        int length = end - glyph;
        if (length > 0) {
            FillResultRects(this, page, glyph, length);
        }
    }
}

static int CountDigits(Str text, int& byteIdx, int maxCount, int (*step)(Str, int&)) {
    int count = 0;
    while (count < maxCount) {
        int nextByte = byteIdx;
        if (!isDigit(step(text, nextByte))) {
            break;
        }
        byteIdx = nextByte;
        count++;
    }
    return count;
}

// Extend either boundary across comma-separated digit groups, e.g. "1,234,567".
static int ExtendAcrossCommaGroups(Str text, int textLen, int pos, int dir) {
    auto step = dir < 0 ? Utf8CodepointPrev : Utf8CodepointNext;
    int posByte = Utf8CodepointToByteIndex(text, pos);
    while (dir < 0 ? pos >= 2 : pos < textLen) {
        int commaByte = posByte;
        if (step(text, commaByte) != ',') {
            break;
        }
        int maxDigits = dir < 0 ? pos - 1 : textLen - pos - 1;
        int nDigits = CountDigits(text, commaByte, maxDigits, step);
        if (nDigits == 0) {
            break;
        }
        pos += dir * (nDigits + 1);
        posByte = commaByte;
    }
    return pos;
}

void TextSelection::GetWordBoundsAt(int pageNo, double x, double y, int* wordStartOut, int* wordEndOut) {
    int i = FindClosestGlyphAt(pageNo, x, y);
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen);

    bool isAllDigits = true;
    int c = 0;
    int iByte = Utf8CodepointToByteIndex(text, i);
    int cByte = iByte;
    for (; i > 0;) {
        int prevByte = iByte;
        c = Utf8CodepointPrev(text, prevByte);
        if (!isWordChar(c)) {
            cByte = prevByte;
            break;
        }
        if (!isDigit(c)) {
            isAllDigits = false;
        }
        iByte = prevByte;
        i--;
    }
    int wordStart = i;
    int maybeNumberStart = i;
    if (isAllDigits && (c == '.' || c == ',')) {
        // walk backward across a pattern like "1,234." or "1,234,567,"
        int nDigits = CountDigits(text, cByte, i - 1, Utf8CodepointPrev);
        if (nDigits > 0) {
            maybeNumberStart = i - nDigits - 1;
            // continue backward across comma-separated groups
            maybeNumberStart = ExtendAcrossCommaGroups(text, textLen, maybeNumberStart, -1);
        } else {
            isAllDigits = false;
        }
    }

    for (; i < textLen;) {
        int nextByte = iByte;
        c = Utf8CodepointNext(text, nextByte);
        if (!isWordChar(c)) {
            break;
        }
        if (!isDigit(c)) {
            isAllDigits = false;
        }
        iByte = nextByte;
        i++;
    }

    // try to select numbers with commas and decimal points
    // e.g. "1,234.56" or "1,234,567" or "123.45"
    int wordEnd = i;
    if (isAllDigits) {
        // extend forward across comma groups
        wordEnd = ExtendAcrossCommaGroups(text, textLen, wordEnd, 1);
        // extend forward across decimal point + digits
        int wordEndByte = Utf8CodepointToByteIndex(text, wordEnd);
        int dotEndByte = wordEndByte;
        if (wordEnd < textLen && Utf8CodepointNext(text, dotEndByte) == '.') {
            int nDigits = CountDigits(text, dotEndByte, textLen - wordEnd - 1, Utf8CodepointNext);
            if (nDigits > 0) {
                wordEnd += nDigits + 1;
            }
        }
        // extend backward across comma groups
        wordStart = ExtendAcrossCommaGroups(text, textLen, wordStart, -1);
        wordStart = std::min(maybeNumberStart, wordStart);
    }
    *wordStartOut = wordStart;
    *wordEndOut = wordEnd;
}

void TextSelection::SelectWordAt(int pageNo, double x, double y) {
    int wordStart = 0, wordEnd = 0;
    GetWordBoundsAt(pageNo, x, y, &wordStart, &wordEnd);
    // remember the word as the anchor for word-granular drag extension
    wordStartPage = pageNo;
    wordStartGlyph = wordStart;
    wordEndPage = pageNo;
    wordEndGlyph = wordEnd;
    StartAt(pageNo, wordStart);
    SelectUpTo(pageNo, wordEnd);
}

// Empty-box newlines end lines; empty-box spaces remain selectable (#5712).
static int FindLineBoundary(Str text, Rect* coords, int pos, int textLen, int dir) {
    auto step = dir < 0 ? Utf8CodepointPrev : Utf8CodepointNext;
    int byteIdx = Utf8CodepointToByteIndex(text, pos);
    while (dir < 0 ? pos > 0 : pos < textLen) {
        int nextByte = byteIdx;
        int c = step(text, nextByte);
        int glyph = dir < 0 ? pos - 1 : pos;
        if (c == '\n' && !coords[glyph].x && !coords[glyph].dx) {
            break;
        }
        pos += dir;
        byteIdx = nextByte;
    }
    return pos;
}

// select the whole line of text at (x, y) (triple-click; issue #694)
void TextSelection::SelectLineAt(int pageNo, double x, double y) {
    int i = FindClosestGlyphAt(pageNo, x, y);
    if (i < 0) {
        return;
    }
    Rect* coords;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    int lineStart = FindLineBoundary(text, coords, i, textLen, -1);
    int lineEnd = FindLineBoundary(text, coords, i, textLen, 1);
    StartAt(pageNo, lineStart);
    SelectUpTo(pageNo, lineEnd);
}

// (pageA, glyphA) is before (pageB, glyphB) in reading order
static bool PosBefore(int pageA, int glyphA, int pageB, int glyphB) {
    if (pageA != pageB) {
        return pageA < pageB;
    }
    return glyphA < glyphB;
}

// extend the selection so it spans whole words from the anchor word (set by
// the last SelectWordAt) to the word at (x, y)
void TextSelection::SelectWordsUpTo(int pageNo, double x, double y) {
    // no anchor word yet (shouldn't happen) - fall back to glyph selection
    if (wordStartGlyph == -1) {
        SelectUpTo(pageNo, x, y);
        return;
    }
    int cursorStart = 0, cursorEnd = 0;
    GetWordBoundsAt(pageNo, x, y, &cursorStart, &cursorEnd);

    // union the anchor word with the word under the cursor, so the selection
    // always covers whole words from the lower to the upper of the two
    int startPg = wordStartPage, startGl = wordStartGlyph;
    if (PosBefore(pageNo, cursorStart, startPg, startGl)) {
        startPg = pageNo;
        startGl = cursorStart;
    }
    int endPg = wordEndPage, endGl = wordEndGlyph;
    if (PosBefore(endPg, endGl, pageNo, cursorEnd)) {
        endPg = pageNo;
        endGl = cursorEnd;
    }
    StartAt(startPg, startGl);
    SelectUpTo(endPg, endGl);
}

void TextSelection::CopySelection(TextSelection* orig) {
    Reset();
    StartAt(orig->startPage, orig->startGlyph);
    SelectUpTo(orig->endPage, orig->endGlyph);
}

TempStr TextSelection::ExtractTextTemp(Str lineSep) {
    StrVec lines;

    int fromPage, fromGlyph, toPage, toGlyph;
    GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);

    // callers joining with " " want one flat line, not restored indentation
    bool layout = str::ContainsChar(lineSep, '\n');
    for (int page = fromPage; page <= toPage; page++) {
        int textLen;
        engine->GetTextForPage(page, &textLen);
        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0) {
            FillResultRects(this, page, glyph, length, &lines, layout);
        }
    }

    return JoinTemp(&lines, lineSep);
}

void TextSelection::GetGlyphRange(int* fromPage, int* fromGlyph, int* toPage, int* toGlyph) const {
    bool reverse = PosBefore(endPage, endGlyph, startPage, startGlyph);
    *fromPage = reverse ? endPage : startPage;
    *fromGlyph = reverse ? endGlyph : startGlyph;
    *toPage = reverse ? startPage : endPage;
    *toGlyph = reverse ? startGlyph : endGlyph;
}

// Cross a page boundary, landing at its first or last glyph.
static bool MoveTextPage(EngineBase* engine, int& page, int& glyph, int dir, int nPages) {
    if (dir == 0 || (dir > 0 ? page >= nPages : page <= 1)) {
        return false;
    }
    page += dir > 0 ? 1 : -1;
    glyph = 0;
    if (dir < 0) {
        engine->GetTextForPage(page, &glyph);
    }
    return true;
}

// Move free end (page, glyph) by one glyph in reading order. dir +1 / -1.
static bool MoveFreeEndByGlyph(EngineBase* engine, int& page, int& glyph, int dir) {
    int nPages = engine->PageCount();
    int textLen = 0;
    engine->GetTextForPage(page, &textLen);
    if (dir > 0 && glyph < textLen) {
        glyph++;
        return true;
    }
    if (dir <= 0 && glyph > 0) {
        glyph--;
        return true;
    }
    return MoveTextPage(engine, page, glyph, dir > 0 ? 1 : -1, nPages);
}

// Move free end (page, glyph) to the previous / next word boundary. dir +1 / -1.
// Steps off the current position, then over any run of non-word characters, then
// to the far side of the word it lands in - i.e. what Ctrl+Left / Ctrl+Right do
// in a text editor. Stops at a page boundary so a single step never skips a page.
static bool MoveFreeEndByWord(EngineBase* engine, int& page, int& glyph, int dir) {
    int textLen = 0;
    Str text = engine->GetTextForPage(page, &textLen);
    if (textLen <= 0) {
        return MoveFreeEndByGlyph(engine, page, glyph, dir);
    }
    auto charAt = [&](int ix) -> int {
        int byteIdx = Utf8CodepointToByteIndex(text, ix);
        int next = byteIdx;
        return Utf8CodepointNext(text, next);
    };

    int fromPage = page;
    if (!MoveFreeEndByGlyph(engine, page, glyph, dir)) {
        return false;
    }
    if (page != fromPage) {
        return true;
    }
    // the character we are moving toward decides whether we're still in a word
    while (glyph > 0 && glyph < textLen && !isWordChar(charAt(dir < 0 ? glyph - 1 : glyph))) {
        glyph += dir;
    }
    while (glyph > 0 && glyph < textLen && isWordChar(charAt(dir < 0 ? glyph - 1 : glyph))) {
        glyph += dir;
    }
    return true;
}

// True if glyph i is a zero-width newline (line break in the page text stream).
static bool IsLineBreakAt(Str text, Rect* coords, int i, int textLen) {
    if (i < 0 || i >= textLen || !coords) {
        return false;
    }
    if (coords[i].x || coords[i].dx) {
        return false;
    }
    int byteIdx = Utf8CodepointToByteIndex(text, i);
    int nextByte = byteIdx;
    int c = Utf8CodepointNext(text, nextByte);
    return c == '\n';
}

// Move free end by one visual line (same x when possible). dir +1 = next line.
static bool MoveFreeEndByLine(EngineBase* engine, int& page, int& glyph, int dir) {
    int nPages = engine->PageCount();
    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(page, &textLen, &coords);
    if (textLen <= 0 || !coords) {
        return MoveTextPage(engine, page, glyph, dir, nPages);
    }

    // reference point: center of the glyph left of the free end (or first glyph)
    int refIx = ClampI(glyph, 1, textLen) - 1;
    while (refIx > 0 && !coords[refIx].x && !coords[refIx].dx && !IsLineBreakAt(text, coords, refIx, textLen)) {
        refIx--;
    }
    int refX = coords[refIx].x + (coords[refIx].dx / 2);
    int refY = coords[refIx].y + (coords[refIx].dy / 2);
    int lineH = coords[refIx].dy > 0 ? coords[refIx].dy : 12;
    int ySlop = std::max(lineH / 2, 2);

    int bestIx = -1;
    int bestDist = INT_MAX;
    int targetBandY = -1;

    bool forward = dir > 0;
    for (int i = 0; i < textLen; i++) {
        if (!coords[i].x && !coords[i].dx) {
            continue;
        }
        int cy = coords[i].y + (coords[i].dy / 2);
        if (forward ? cy <= refY + ySlop : cy >= refY - ySlop) {
            continue;
        }
        if (targetBandY < 0 || (forward ? cy < targetBandY : cy > targetBandY)) {
            targetBandY = cy;
        }
    }
    if (targetBandY < 0) {
        return MoveTextPage(engine, page, glyph, dir, nPages);
    }
    for (int i = 0; i < textLen; i++) {
        if (!coords[i].x && !coords[i].dx) {
            continue;
        }
        int cy = coords[i].y + (coords[i].dy / 2);
        if (std::abs(cy - targetBandY) > ySlop) {
            continue;
        }
        int cx = coords[i].x + (coords[i].dx / 2);
        int d = std::abs(cx - refX);
        if (d < bestDist) {
            bestDist = d;
            bestIx = i;
        }
    }
    if (bestIx < 0) {
        return false;
    }
    glyph = bestIx + 1;
    return true;
}

// Move a (page, glyph) position one unit in reading order, without touching any
// selection. Keyboard selection drives its caret with this; ExtendBy() moves the
// selection's free end with the same steps.
bool TextPosMoveBy(EngineBase* engine, int& page, int& glyph, TextSelectUnit unit, int dir) {
    if (!engine || page < 1 || glyph < 0 || dir == 0) {
        return false;
    }
    int d = dir > 0 ? 1 : -1;
    if (unit == TextSelectUnit::Glyph) {
        return MoveFreeEndByGlyph(engine, page, glyph, d);
    }
    if (unit == TextSelectUnit::Word) {
        return MoveFreeEndByWord(engine, page, glyph, d);
    }
    return MoveFreeEndByLine(engine, page, glyph, d);
}

// Move the free end (endPage/endGlyph) by delta units in reading order.
// delta > 0 toward document end, delta < 0 toward document start.
// Returns true if the free end moved. Platform code maps keys to unit+delta.
bool TextSelection::ExtendBy(TextSelectUnit unit, int delta) {
    if (!engine || startPage < 1 || endPage < 1 || delta == 0) {
        return false;
    }
    if (startGlyph < 0 || endGlyph < 0) {
        return false;
    }

    int page = endPage;
    int glyph = endGlyph;
    int steps = delta > 0 ? delta : -delta;
    int dir = delta > 0 ? 1 : -1;

    for (int s = 0; s < steps; s++) {
        if (!TextPosMoveBy(engine, page, glyph, unit, dir)) {
            break;
        }
    }

    if (page == endPage && glyph == endGlyph) {
        return false;
    }
    SelectUpTo(page, glyph);
    return true;
}
