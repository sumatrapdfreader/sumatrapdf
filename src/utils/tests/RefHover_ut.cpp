/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Unit tests for the popup region detectors in RefHoverDetect. Each test
// constructs a synthetic per-glyph (text + coords) array that mimics what
// EngineBase::GetTextForPage produces, then asserts the detector returns a
// region matching the documented behaviour.

#include "utils/BaseUtil.h"
#include "RefHoverDetect.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

constexpr float kPageW = 612.f;
constexpr float kPageH = 792.f;
constexpr int kCharW = 6;
constexpr int kLineH = 12;

// Helper: append the WCHARs of `s` starting at (x, y) with fixed-width glyphs.
// Caller passes pre-allocated text/coords buffers and the current length.
static void AddText(WCHAR* text, Rect* coords, int& len, int cap, const WCHAR* s, int x, int y) {
    for (int i = 0; s[i] && len < cap; i++) {
        text[len] = s[i];
        coords[len] = Rect{x + i * kCharW, y, kCharW, kLineH};
        len++;
    }
}

static RectF Mediabox() {
    return RectF{0.f, 0.f, kPageW, kPageH};
}

static bool IsEmpty(RectF r) {
    return r.dx <= 0.f || r.dy <= 0.f;
}

// (1) Sparse-text destination page (< 50 non-empty glyphs): DetectEntryBox
// returns the whole page so an image-heavy page renders fully.
static void SparseTextReturnsWholePage() {
    WCHAR text[64];
    Rect coords[64];
    int len = 0;
    AddText(text, coords, len, 64, L"Heading only", 100, 100);
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 100.f, 100.f);
    utassert(box.x == 0.f);
    utassert(box.y == 0.f);
    utassert(box.dx == kPageW);
    utassert(box.dy == kPageH);
}

// (2) destY < 0 with rich text: DetectEntryBox delegates to LandscapeBox,
// which returns a full-width strip anchored at the page top.
static void NegativeDestYFallsToLandscape() {
    WCHAR text[512];
    Rect coords[512];
    int len = 0;
    for (int i = 0; i < 10; i++) {
        AddText(text, coords, len, 512, L"Body text line content here.", 72, 100 + i * 14);
    }
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, -1.f);
    utassert(box.x == 0.f);
    utassert(box.y == 0.f);
    utassert(box.dx == kPageW);
    utassert(box.dy > 0.f);
    utassert(box.dy < kPageH);
}

// (3) Bracket-style bibliography "[Foo10]" / "[Bar11]": DetectEntryBox fits
// to the first entry and does not include the second.
static void BracketEntryFitsToOneEntry() {
    WCHAR text[512];
    Rect coords[512];
    int len = 0;
    // Entry 1 at y=200, two lines (line 2 indented).
    AddText(text, coords, len, 512, L"[Foo10] Smith J., 2010, Some title.", 72, 200);
    AddText(text, coords, len, 512, L"continuation line of the entry.", 92, 215);
    // Entry 2 at y=240, sibling start back at x=72.
    AddText(text, coords, len, 512, L"[Bar11] Doe J., 2011, Another title.", 72, 240);
    AddText(text, coords, len, 512, L"continuation line of the entry.", 92, 255);
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // Box should end before entry 2 starts at y=240.
    utassert(box.y + box.dy < 240.f);
    // Box should start at or after entry 1's first line.
    utassert(box.y <= 200.f + 6.f);
    utassert(box.y >= 200.f - 12.f);
}

// (3d) Author-year hanging-indent bibliography (no "[N]" markers): the next
// entry is detected by the indent change — a new line back at the entry's
// first-line X after an indented continuation line (rule (b)).
static void AuthorYearEntryFitsToOneEntry() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    AddText(text, coords, len, 1024, L"Smith, J. (2010). Some title here.", 72, 200);
    AddText(text, coords, len, 1024, L"Journal of Things, 12(3), 45-67.", 92, 215);
    AddText(text, coords, len, 1024, L"Doe, A. (2011). Another work title.", 72, 240);
    AddText(text, coords, len, 1024, L"Other Journal, 4(2), 89-101.", 92, 255);
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // both lines of entry 1, nothing of entry 2
    utassert(box.y <= 200.f && box.y >= 188.f);
    utassert(box.y + box.dy > 215.f);
    utassert(box.y + box.dy < 240.f);
    utassert(box.dx < kPageW);
}

// (3e) Single-line description-list entries (abbreviation lists) with normal
// leading: the next entry is detected by a new line back at the entry's X
// before any continuation indent was seen (rule (d)).
static void SingleLineEntryList() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    AddText(text, coords, len, 1024, L"JVM Java Virtual Machine. 19, 36", 72, 200);
    AddText(text, coords, len, 1024, L"LLM Large Language Model. 45", 72, 215);
    AddText(text, coords, len, 1024, L"PDF Portable Document Format. 7", 72, 230);
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // first entry only: the second line's glyphs (bottom 227) are excluded
    utassert(box.y + box.dy < 227.f);
    utassert(box.dx < kPageW);
}

// (3f) Single-line entries separated by blank lines: the vertical paragraph
// gap ends the entry (rule (c)).
static void GapSeparatedEntryList() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    AddText(text, coords, len, 1024, L"First footnote text goes here.", 72, 200);
    AddText(text, coords, len, 1024, L"Second footnote starts here.", 72, 240);
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    utassert(box.y + box.dy < 240.f);
    utassert(box.dx < kPageW);
}

// (3a) 2-column layout, entry in the right column: the detected box must not
// extend into the left column's same-y body text (the start-of-line walk must
// not cross the column gutter).
static void TwoColumnRightEntryStaysInColumn() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    // Left column body text at x=72 (right edge ≈ 72+29*6=246), same y range.
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, len, 1024, L"left column body text line...", 72, 200 + i * 15);
    }
    // Right column starts at x=320 (gutter ≈ 246..320).
    AddText(text, coords, len, 1024, L"[Foo10] Smith J., 2010, Title.", 320, 200);
    AddText(text, coords, len, 1024, L"continuation of the entry.", 340, 215);
    AddText(text, coords, len, 1024, L"[Bar11] Doe J., 2011, Other.", 320, 240);
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 320.f, 200.f);
    utassert(!IsEmpty(box));
    // Box must stay right of the gutter (not include the left column).
    utassert(box.x > 246.f);
    // And still end before the sibling entry.
    utassert(box.y + box.dy < 240.f);
}

// (3b) 2-column layout, entry in the left column: the detected box must not
// extend into the right column's same-y text.
static void TwoColumnLeftEntryStaysInColumn() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    // Left column entry at x=72 (line right edge ≈ 72+29*6=246).
    AddText(text, coords, len, 1024, L"[Foo10] Smith J., 2010, Title", 72, 200);
    AddText(text, coords, len, 1024, L"continuation of the entry.", 92, 215);
    AddText(text, coords, len, 1024, L"[Bar11] Doe J., 2011, Other.", 72, 240);
    // Right column body text at x=340, same y range.
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, len, 1024, L"right column body text line..", 340, 200 + i * 15);
    }
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // Box must not reach into the right column at x=340.
    utassert(box.x + box.dx < 340.f);
    utassert(box.y + box.dy < 240.f);
}

// (3c) All-caps accented Spanish heading "SECCIÓN 2": the heading-prefix
// dictionary must match case-insensitively beyond ASCII (the process runs in
// the "C" locale where towlower doesn't fold Ó), so the destination is
// detected as a heading and routed to the full-width landscape view rather
// than fitted like a bibliography entry.
static void AccentedAllCapsHeadingDetected() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    AddText(text, coords, len, 1024, L"SECCIÓN 2 RESULTADOS", 72, 200);
    // body paragraph: first line indented, second back at the margin
    AddText(text, coords, len, 1024, L"texto del cuerpo del documento.", 92, 215);
    AddText(text, coords, len, 1024, L"continua en el margen izquierdo.", 72, 230);
    for (int i = 0; i < 3; i++) {
        AddText(text, coords, len, 1024, L"mas lineas de texto del cuerpo.", 72, 245 + i * 15);
    }
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // heading destination => landscape view spanning the full page width
    utassert(box.x == 0.f);
    utassert(box.dx == kPageW);
}

// (4) Equation label "(14)" at right column edge near destY: DetectEquationBox
// returns a tight, full-width strip near the label.
static void EquationLabelDetected() {
    WCHAR text[512];
    Rect coords[512];
    int len = 0;
    // Equation row at y=300, label "(14)" at x≈540 (right of mediabox.dx*0.5).
    AddText(text, coords, len, 512, L"dH/dt = -sum( ... )", 200, 300);
    AddText(text, coords, len, 512, L"(14)", 540, 300);
    // A paragraph below.
    for (int i = 0; i < 3; i++) {
        AddText(text, coords, len, 512, L"Paragraph text below the equation here.", 72, 330 + i * 14);
    }
    RectF box = DetectEquationBox(text, coords, len, Mediabox(), 72.f, 300.f);
    utassert(!IsEmpty(box));
    utassert(box.x == 0.f);
    utassert(box.dx == kPageW);
    // Tight vertical band around y=300 (within ~3 line heights).
    utassert(box.y < 300.f);
    utassert(box.y + box.dy < 300.f + 3 * (float)kLineH + 20.f);
}

// (5) "(N)" sitting in the left half of the page (body-text marker, not a
// display-eq label): DetectEquationBox returns empty. The label is alone on
// its line (line-trailing), so only the left-half rule can reject it — this
// pins that rule specifically.
static void BodyTextParenRejected() {
    WCHAR text[512];
    Rect coords[512];
    int len = 0;
    AddText(text, coords, len, 512, L"some body text on another line.", 72, 280);
    // line-trailing "(14)" at x=80 — left half of the 612-wide page
    AddText(text, coords, len, 512, L"(14)", 80, 300);
    RectF box = DetectEquationBox(text, coords, len, Mediabox(), 80.f, 300.f);
    utassert(IsEmpty(box));
}

// (6) "(N)" with more text further right on the same line: DetectEquationBox
// returns empty (label must be line-trailing).
static void NonTrailingParenRejected() {
    WCHAR text[512];
    Rect coords[512];
    int len = 0;
    // Label "(14)" at x=540 followed by text at x=560 (further right).
    AddText(text, coords, len, 512, L"dH/dt = ...", 200, 300);
    AddText(text, coords, len, 512, L"(14)", 540, 300);
    AddText(text, coords, len, 512, L"trail", 560, 300);
    RectF box = DetectEquationBox(text, coords, len, Mediabox(), 200.f, 300.f);
    utassert(IsEmpty(box));
}

// (7) Empty / null inputs: DetectEquationBox returns empty rect, never crashes.
static void EmptyInputsHandled() {
    RectF box1 = DetectEquationBox(nullptr, nullptr, 0, Mediabox(), 0.f, 100.f);
    utassert(IsEmpty(box1));
    WCHAR text[1] = {0};
    Rect coords[1]{};
    RectF box2 = DetectEquationBox(text, coords, 0, Mediabox(), 0.f, 100.f);
    utassert(IsEmpty(box2));
    // destY <= 0 short-circuit.
    RectF box3 = DetectEquationBox(text, coords, 0, Mediabox(), 0.f, -1.f);
    utassert(IsEmpty(box3));
}

// (8a) Caption-detection table covers non-English label words: a "Tableau 2"
// French caption above the destination triggers the upward figure-body
// extension (region top moves above destY).
static void FrenchCaptionDetected() {
    WCHAR text[512];
    Rect coords[512];
    int len = 0;
    // Caption line "Tableau 2: Données" at y=300 — this is the destY.
    AddText(text, coords, len, 512, L"Tableau 2: Donnees", 72, 300);
    // Body paragraph below.
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, len, 512, L"Paragraphe de texte courant.", 72, 320 + i * 14);
    }
    RectF box = LandscapeBox(Mediabox(), 72.f, 300.f, text, coords, len);
    // destAtCaption pulls region top above destY (figure body extension).
    utassert(box.y < 300.f - 50.f);
    utassert(box.dx == kPageW);
}

// (8b) Italian / Portuguese caption words ("Tabella 2", "Tabela 2") are in
// the dictionary, as the es/it/pt comment claims.
static void ItalianPortugueseCaptionDetected() {
    const WCHAR* captions[] = {L"Tabella 2: Dati", L"Tabela 2: Dados"};
    for (const WCHAR* caption : captions) {
        WCHAR text[512];
        Rect coords[512];
        int len = 0;
        AddText(text, coords, len, 512, caption, 72, 300);
        for (int i = 0; i < 5; i++) {
            AddText(text, coords, len, 512, L"testo del corpo del documento.", 72, 320 + i * 14);
        }
        RectF box = LandscapeBox(Mediabox(), 72.f, 300.f, text, coords, len);
        // destAtCaption pulls region top above destY (figure body extension)
        utassert(box.y < 300.f - 50.f);
    }
}

// (10) A bibliography entry starting with "Sections of ..." must not be
// mistaken for a "Section N" heading (the dictionary match requires a
// trailing word boundary): the entry box stays fitted, not landscape.
static void PluralHeadingWordNotMatched() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    AddText(text, coords, len, 1024, L"Sections of papers, J. Smith.", 72, 200);
    AddText(text, coords, len, 1024, L"continuation line of the entry.", 92, 215);
    AddText(text, coords, len, 1024, L"Another entry begins here now.", 72, 240);
    RectF box = DetectEntryBox(text, coords, len, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // fitted entry box, not the full-width landscape heading view
    utassert(box.dx < kPageW);
    utassert(box.y + box.dy < 240.f);
}

// (11) Caption-extension picks the topmost caption below the region, not the
// first one in glyph-array order (PDFs draw text in arbitrary order).
static void CaptionExtensionPicksTopmost() {
    WCHAR text[1024];
    Rect coords[1024];
    int len = 0;
    for (int i = 0; i < 3; i++) {
        AddText(text, coords, len, 1024, L"body paragraph at the dest.", 72, 100 + i * 14);
    }
    // a far caption drawn EARLY in the content stream ... (the leading
    // space stands in for the inter-block separator of real extraction)
    AddText(text, coords, len, 1024, L" Figure 9: far away caption", 72, 700);
    // ... and a nearer caption drawn later
    AddText(text, coords, len, 1024, L" Figure 2: near caption", 72, 420);
    RectF box = LandscapeBox(Mediabox(), 72.f, 100.f, text, coords, len);
    // region must extend to the near caption only, not down to y=700
    utassert(box.y + box.dy > 420.f);
    utassert(box.y + box.dy < 600.f);
}

// (9) LandscapeBox: returned region spans the full mediabox width, starts at
// destY-margin, height is positive and bounded.
static void LandscapeBoxBasicShape() {
    WCHAR text[256];
    Rect coords[256];
    int len = 0;
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, len, 256, L"some body content.", 72, 400 + i * 14);
    }
    RectF box = LandscapeBox(Mediabox(), 72.f, 400.f, text, coords, len);
    utassert(box.x == 0.f);
    utassert(box.dx == kPageW);
    utassert(box.y >= 0.f);
    utassert(box.y < 400.f);
    utassert(box.dy > 0.f);
    utassert(box.y + box.dy <= kPageH);
}

void RefHoverTest() {
    AccentedAllCapsHeadingDetected();
    AuthorYearEntryFitsToOneEntry();
    BodyTextParenRejected();
    BracketEntryFitsToOneEntry();
    CaptionExtensionPicksTopmost();
    GapSeparatedEntryList();
    SingleLineEntryList();
    EmptyInputsHandled();
    EquationLabelDetected();
    FrenchCaptionDetected();
    ItalianPortugueseCaptionDetected();
    LandscapeBoxBasicShape();
    NegativeDestYFallsToLandscape();
    NonTrailingParenRejected();
    PluralHeadingWordNotMatched();
    SparseTextReturnsWholePage();
    TwoColumnLeftEntryStaysInColumn();
    TwoColumnRightEntryStaysInColumn();
}
