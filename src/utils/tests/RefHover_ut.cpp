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
// display-eq label): DetectEquationBox returns empty.
static void BodyTextParenRejected() {
    WCHAR text[512];
    Rect coords[512];
    int len = 0;
    // "(14)" at x=80 — left half of 612-wide page.
    AddText(text, coords, len, 512, L"(14) inline text continuing past the label.", 80, 300);
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
    BodyTextParenRejected();
    BracketEntryFitsToOneEntry();
    EmptyInputsHandled();
    EquationLabelDetected();
    FrenchCaptionDetected();
    LandscapeBoxBasicShape();
    NegativeDestYFallsToLandscape();
    NonTrailingParenRejected();
    SparseTextReturnsWholePage();
    TwoColumnLeftEntryStaysInColumn();
    TwoColumnRightEntryStaysInColumn();
}
