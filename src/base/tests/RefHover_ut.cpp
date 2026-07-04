/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Unit tests for the popup region detectors in RefHoverDetect. Each test
// constructs a synthetic per-glyph (text + coords) array that mimics what
// EngineBase::GetTextForPage produces, then asserts the detector returns a
// region matching the documented behaviour.

#include "base/Base.h"
#include "RefHoverDetect.h"
#include "RefHoverTextDetect.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

constexpr float kPageW = 612.f;
constexpr float kPageH = 792.f;
constexpr int kCharW = 6;
constexpr int kLineH = 12;

// Helper: append the WCHARs of `s` starting at (x, y) with fixed-width glyphs.
// Caller passes pre-allocated text/coords buffers and the current length.
static void AddText(WCHAR* text, Rect* coords, int& n, int cap, WStr s, int x, int y) {
    for (int i = 0; i < s.len && n < cap; i++) {
        text[n] = s.s[i];
        coords[n] = Rect{x + i * kCharW, y, kCharW, kLineH};
        n++;
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
    int n = 0;
    AddText(text, coords, n, 64, L"Heading only", 100, 100);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 100.f, 100.f);
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
    int n = 0;
    for (int i = 0; i < 10; i++) {
        AddText(text, coords, n, 512, L"Body text line content here.", 72, 100 + i * 14);
    }
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, -1.f);
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
    int n = 0;
    // Entry 1 at y=200, two lines (line 2 indented).
    AddText(text, coords, n, 512, L"[Foo10] Smith J., 2010, Some title.", 72, 200);
    AddText(text, coords, n, 512, L"continuation line of the entry.", 92, 215);
    // Entry 2 at y=240, sibling start back at x=72.
    AddText(text, coords, n, 512, L"[Bar11] Doe J., 2011, Another title.", 72, 240);
    AddText(text, coords, n, 512, L"continuation line of the entry.", 92, 255);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
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
    int n = 0;
    AddText(text, coords, n, 1024, L"Smith, J. (2010). Some title here.", 72, 200);
    AddText(text, coords, n, 1024, L"Journal of Things, 12(3), 45-67.", 92, 215);
    AddText(text, coords, n, 1024, L"Doe, A. (2011). Another work title.", 72, 240);
    AddText(text, coords, n, 1024, L"Other Journal, 4(2), 89-101.", 92, 255);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
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
    int n = 0;
    AddText(text, coords, n, 1024, L"JVM Java Virtual Machine. 19, 36", 72, 200);
    AddText(text, coords, n, 1024, L"LLM Large Language Model. 45", 72, 215);
    AddText(text, coords, n, 1024, L"PDF Portable Document Format. 7", 72, 230);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
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
    int n = 0;
    AddText(text, coords, n, 1024, L"First footnote text goes here.", 72, 200);
    AddText(text, coords, n, 1024, L"Second footnote starts here.", 72, 240);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
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
    int n = 0;
    // Left column body text at x=72 (right edge ≈ 72+29*6=246), same y range.
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, n, 1024, L"left column body text line...", 72, 200 + i * 15);
    }
    // Right column starts at x=320 (gutter ≈ 246..320).
    AddText(text, coords, n, 1024, L"[Foo10] Smith J., 2010, Title.", 320, 200);
    AddText(text, coords, n, 1024, L"continuation of the entry.", 340, 215);
    AddText(text, coords, n, 1024, L"[Bar11] Doe J., 2011, Other.", 320, 240);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 320.f, 200.f);
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
    int n = 0;
    // Left column entry at x=72 (line right edge ≈ 72+29*6=246).
    AddText(text, coords, n, 1024, L"[Foo10] Smith J., 2010, Title", 72, 200);
    AddText(text, coords, n, 1024, L"continuation of the entry.", 92, 215);
    AddText(text, coords, n, 1024, L"[Bar11] Doe J., 2011, Other.", 72, 240);
    // Right column body text at x=340, same y range.
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, n, 1024, L"right column body text line..", 340, 200 + i * 15);
    }
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // Box must not reach into the right column at x=340.
    utassert(box.x + box.dx < 340.f);
    utassert(box.y + box.dy < 240.f);
}

// (3g) Bracket entry wraps across a 2-column page break ("[63]"-style): the
// last entry in the left column ends near the page bottom with no sibling
// "[" or blank-line gap to close it, and continues at the top of the next
// column. DetectEntryBox must report that continuation via continuationOut
// without swallowing the following entry ("[64]") in the new column.
static void ColumnWrapContinuationDetected() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Left column's last entry: single line near the page bottom (kPageH=792).
    AddText(text, coords, n, 1024, L"[63] Voelter M: DSL Engineering des", 72, 762);
    // Right column (gutter ~252..340): continuation of [63] at the top (no
    // bracket label), then the next real entry [64] further down.
    AddText(text, coords, n, 1024, L"Languages dslbook org Germany 2013", 340, 40);
    AddText(text, coords, n, 1024, L"[64] Moretti N Xie X et al Title", 340, 65);
    RectF continuation{};
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 762.f, &continuation);
    utassert(!IsEmpty(box));
    // Primary box unaffected: stays in the left column, at the entry's line.
    utassert(box.x < 246.f);
    utassert(box.y <= 762.f + 6.f);
    utassert(!IsEmpty(continuation));
    // Continuation sits in the right column, above the next entry.
    utassert(continuation.x > 246.f);
    utassert(continuation.y <= 40.f + 6.f);
    utassert(continuation.y + continuation.dy < 65.f);
}

// (3h) Last entry in the left column ends near the column's content edge, but
// the top of the next column is unrelated running body text (e.g. a
// Discussion section sharing the page with References, not a wrap of this
// entry) that only reaches a bracket many lines down. DetectEntryBox must not
// mistake that long unrelated block for a short wrapped tail.
static void ColumnWrapContinuationRejectsUnrelatedBodyText() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    AddText(text, coords, n, 1024, L"[47] Zhang X Wang Y Wang J Title A", 72, 750);
    // Right column: several lines of unrelated running text, then a sibling
    // entry far past the short continuation cap.
    for (int i = 0; i < 6; i++) {
        AddText(text, coords, n, 1024, L"unrelated running body text here.", 340, 40 + i * 14);
    }
    AddText(text, coords, n, 1024, L"[48] Li H Hou L Wang X Guo H Title", 340, 130);
    RectF continuation{};
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 750.f, &continuation);
    utassert(!IsEmpty(box));
    utassert(IsEmpty(continuation));
}

// (3c) All-caps accented Spanish heading "SECCIÓN 2": the heading-prefix
// dictionary must match case-insensitively beyond ASCII (the process runs in
// the "C" locale where towlower doesn't fold Ó), so the destination is
// detected as a heading and routed to the full-width landscape view rather
// than fitted like a bibliography entry.
static void AccentedAllCapsHeadingDetected() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    AddText(text, coords, n, 1024, L"SECCIÓN 2 RESULTADOS", 72, 200);
    // body paragraph: first line indented, second back at the margin
    AddText(text, coords, n, 1024, L"texto del cuerpo del documento.", 92, 215);
    AddText(text, coords, n, 1024, L"continua en el margen izquierdo.", 72, 230);
    for (int i = 0; i < 3; i++) {
        AddText(text, coords, n, 1024, L"mas lineas de texto del cuerpo.", 72, 245 + i * 15);
    }
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
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
    int n = 0;
    // Equation row at y=300, label "(14)" at x≈540 (right of mediabox.dx*0.5).
    AddText(text, coords, n, 512, L"dH/dt = -sum( ... )", 200, 300);
    AddText(text, coords, n, 512, L"(14)", 540, 300);
    // A paragraph below.
    for (int i = 0; i < 3; i++) {
        AddText(text, coords, n, 512, L"Paragraph text below the equation here.", 72, 330 + i * 14);
    }
    RectF box = DetectEquationBox(WStr(text, n), coords, Mediabox(), 72.f, 300.f);
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
    int n = 0;
    AddText(text, coords, n, 512, L"some body text on another line.", 72, 280);
    // line-trailing "(14)" at x=80 — left half of the 612-wide page
    AddText(text, coords, n, 512, L"(14)", 80, 300);
    RectF box = DetectEquationBox(WStr(text, n), coords, Mediabox(), 80.f, 300.f);
    utassert(IsEmpty(box));
}

// (6) "(N)" with more text further right on the same line: DetectEquationBox
// returns empty (label must be line-trailing).
static void NonTrailingParenRejected() {
    WCHAR text[512];
    Rect coords[512];
    int n = 0;
    // Label "(14)" at x=540 followed by text at x=560 (further right).
    AddText(text, coords, n, 512, L"dH/dt = ...", 200, 300);
    AddText(text, coords, n, 512, L"(14)", 540, 300);
    AddText(text, coords, n, 512, L"trail", 560, 300);
    RectF box = DetectEquationBox(WStr(text, n), coords, Mediabox(), 200.f, 300.f);
    utassert(IsEmpty(box));
}

// (6b) A line-trailing 4-digit "(2013)" in the right half is a citation year at
// the end of a bibliography line, not a display-equation label: DetectEquationBox
// must reject it (else hovering a reference would render the whole reference row
// as if it were an equation strip). A 2-3 digit label is still detected.
static void BibYearParenNotEquationLabel() {
    WCHAR text[512];
    Rect coords[512];
    int n = 0;
    // Reference line ending in "(2013)" at the right column (right half).
    AddText(text, coords, n, 512, L"O. Zimmermann Decisions IGI Global (2013)", 320, 300);
    RectF box = DetectEquationBox(WStr(text, n), coords, Mediabox(), 320.f, 300.f);
    utassert(IsEmpty(box));
    // sanity: a genuine 2-digit equation label is still detected
    n = 0;
    AddText(text, coords, n, 512, L"some eq body", 200, 300);
    AddText(text, coords, n, 512, L"(14)", 540, 300);
    box = DetectEquationBox(WStr(text, n), coords, Mediabox(), 200.f, 300.f);
    utassert(!IsEmpty(box));
}

// (7) Empty / null inputs: DetectEquationBox returns empty rect, never crashes.
static void EmptyInputsHandled() {
    RectF box1 = DetectEquationBox(WStr{}, nullptr, Mediabox(), 0.f, 100.f);
    utassert(IsEmpty(box1));
    WCHAR text[1] = {0};
    Rect coords[1]{};
    RectF box2 = DetectEquationBox(WStr(text, 0), coords, Mediabox(), 0.f, 100.f);
    utassert(IsEmpty(box2));
    // destY <= 0 short-circuit.
    RectF box3 = DetectEquationBox(WStr(text, 0), coords, Mediabox(), 0.f, -1.f);
    utassert(IsEmpty(box3));
}

// (8a) Caption-detection table covers non-English label words: a "Tableau 2"
// French caption above the destination triggers the upward figure-body
// extension (region top moves above destY).
static void FrenchCaptionDetected() {
    WCHAR text[512];
    Rect coords[512];
    int n = 0;
    // Caption line "Tableau 2: Données" at y=300 — this is the destY.
    AddText(text, coords, n, 512, L"Tableau 2: Donnees", 72, 300);
    // Body paragraph below.
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, n, 512, L"Paragraphe de texte courant.", 72, 320 + i * 14);
    }
    RectF box = LandscapeBox(Mediabox(), 72.f, 300.f, WStr(text, n), coords);
    // destAtCaption pulls region top above destY (figure body extension).
    utassert(box.y < 300.f - 50.f);
    utassert(box.dx == kPageW);
}

// (8b) Italian / Portuguese caption words ("Tabella 2", "Tabela 2") are in
// the dictionary, as the es/it/pt comment claims.
static void ItalianPortugueseCaptionDetected() {
    WStr captions[] = {L"Tabella 2: Dati", L"Tabela 2: Dados"};
    for (WStr caption : captions) {
        WCHAR text[512];
        Rect coords[512];
        int n = 0;
        AddText(text, coords, n, 512, caption, 72, 300);
        for (int i = 0; i < 5; i++) {
            AddText(text, coords, n, 512, L"testo del corpo del documento.", 72, 320 + i * 14);
        }
        RectF box = LandscapeBox(Mediabox(), 72.f, 300.f, WStr(text, n), coords);
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
    int n = 0;
    AddText(text, coords, n, 1024, L"Sections of papers, J. Smith.", 72, 200);
    AddText(text, coords, n, 1024, L"continuation line of the entry.", 92, 215);
    AddText(text, coords, n, 1024, L"Another entry begins here now.", 72, 240);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
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
    int n = 0;
    for (int i = 0; i < 3; i++) {
        AddText(text, coords, n, 1024, L"body paragraph at the dest.", 72, 100 + i * 14);
    }
    // a far caption drawn EARLY in the content stream ... (the leading
    // space stands in for the inter-block separator of real extraction)
    AddText(text, coords, n, 1024, L" Figure 9: far away caption", 72, 700);
    // ... and a nearer caption drawn later
    AddText(text, coords, n, 1024, L" Figure 2: near caption", 72, 420);
    RectF box = LandscapeBox(Mediabox(), 72.f, 100.f, WStr(text, n), coords);
    // region must extend to the near caption only, not down to y=700
    utassert(box.y + box.dy > 420.f);
    utassert(box.y + box.dy < 600.f);
}

// (9) LandscapeBox: returned region spans the full mediabox width, starts at
// destY-margin, height is positive and bounded.
static void LandscapeBoxBasicShape() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    for (int i = 0; i < 5; i++) {
        AddText(text, coords, n, 256, L"some body content.", 72, 400 + i * 14);
    }
    RectF box = LandscapeBox(Mediabox(), 72.f, 400.f, WStr(text, n), coords);
    utassert(box.x == 0.f);
    utassert(box.dx == kPageW);
    utassert(box.y >= 0.f);
    utassert(box.y < 400.f);
    utassert(box.dy > 0.f);
    utassert(box.y + box.dy <= kPageH);
}

// (10) Plain-text citation "(Smith et al., 2020)": cursor on "Smith" yields
// surname "Smith" and year 2020.
static void PlainTextCitationDetected() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    AddText(text, coords, n, 256, L"as shown in (Smith et al., 2020) earlier", 72, 200);
    Str surname{};
    int year = 0;
    // Cursor on the 'S' of "Smith" (glyph 13 → x = 72 + 13*6 = 150).
    bool ok = DetectCitationInPageText(WStr(text, n), coords, n, Point{152, 206}, &surname, &year);
    utassert(ok);
    utassert(surname && str::Eq(surname, "Smith"));
    utassert(year == 2020);
    str::Free(surname);
}

// (11) No 4-digit year near the cursor: detection fails, no allocation.
static void PlainTextCitationNoYear() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    AddText(text, coords, n, 256, L"plain body text without any citation", 72, 200);
    Str surname{};
    int year = 0;
    bool ok = DetectCitationInPageText(WStr(text, n), coords, n, Point{100, 206}, &surname, &year);
    utassert(!ok);
    utassert(!surname);
}

// (12) Bibliography page lookup: a line starting with the surname and
// containing the year anchors at the line's first glyph; a surname that is
// not on the page returns false.
static void SurnameFoundOnBibPage() {
    WCHAR text[512];
    Rect coords[512];
    int n = 0;
    AddText(text, coords, n, 512, L"References", 72, 100);
    AddText(text, coords, n, 512, L"Smith, J. (2020). Some title.", 72, 130);
    AddText(text, coords, n, 512, L"continuation of the entry.", 90, 145);
    float x = 0.f, y = 0.f;
    bool ok = FindSurnameInPageText(WStr(text, n), coords, n, WStrL(L"Smith"), 2020, &x, &y);
    utassert(ok);
    utassert(x == 72.f);
    utassert(y == 130.f);
    ok = FindSurnameInPageText(WStr(text, n), coords, n, WStrL(L"Jones"), 2021, &x, &y);
    utassert(!ok);
}

// (13) Numeric "[N]" citation: cursor inside the brackets yields the number;
// a list "[1, 2]" picks the token nearest the cursor; plain text fails.
static void NumericCitationDetected() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    AddText(text, coords, n, 256, L"see [1] for details", 72, 200);
    int num = 0;
    // Cursor on the '1' (glyph 5 → x = 72 + 5*6 = 102).
    bool ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{105, 206}, &num);
    utassert(ok);
    utassert(num == 1);

    n = 0;
    AddText(text, coords, n, 256, L"prior work [1, 2] showed", 72, 200);
    num = 0;
    // Cursor on the '2' (glyph 15 → x = 72 + 15*6 = 162).
    ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{165, 206}, &num);
    utassert(ok);
    utassert(num == 2);

    n = 0;
    AddText(text, coords, n, 256, L"no brackets here at all", 72, 200);
    num = 0;
    ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{100, 206}, &num);
    utassert(!ok);
}

// (13b) Two "[1]" markers on one line get distinct srcRect x spans.
static void NumericCitationSrcRectDistinctOnLine() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    AddText(text, coords, n, 256, L"see [1] and again [1] end", 72, 200);
    Rect first{}, second{};
    int num = 0;
    bool ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{105, 206}, &num, &first);
    utassert(ok);
    utassert(num == 1);
    utassert(first.dx > 0);
    ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{189, 206}, &num, &second);
    utassert(ok);
    utassert(num == 1);
    utassert(second.dx > 0);
    utassert(first.y == second.y);
    utassert(first.x != second.x);
}

// (13c) Numeric citation page range written with an en-dash ("[2, 9–14]"):
// the en-dash must not break bracket detection, and hovering an endpoint
// resolves to that endpoint's number.
static void NumericCitationRangeEnDash() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    // "deploy [2, 9–14]." — en-dash split from "14" so the \x2013 escape does
    // not swallow the following hex digits.
    AddText(text, coords, n, 256,
            L"deploy [2, 9\x2013"
            L"14].",
            72, 200);
    int num = 0;
    // Cursor on the '9' (idx 11 → x = 72 + 11*6 = 138).
    bool ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{141, 206}, &num);
    utassert(ok);
    utassert(num == 9);
    // Cursor on the '4' of "14" (idx 14 → x = 72 + 14*6 = 156).
    num = 0;
    ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{159, 206}, &num);
    utassert(ok);
    utassert(num == 14);
}

// (13d) Numeric citation list that wraps across a line break ("[8, 17,\n18]"):
// bracket detection must span the line break. Hovering a number on either the
// first or the second line resolves to that number. The 2nd line sits 20pt
// below (> the 16pt single-line tolerance) so this exercises the multi-line
// walk, not the within-line tolerance.
static void NumericCitationLineBreakList() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    AddText(text, coords, n, 256, L"in AECO [8, 17,", 72, 200); // '[' at idx 8 (x=120)
    AddText(text, coords, n, 256, L"18]. These", 72, 220);      // '1' at idx 15 (x=72,y=220)
    int num = 0;
    // Cursor on the '8' on line 1 (idx 9 → x = 72 + 9*6 = 126).
    bool ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{129, 206}, &num);
    utassert(ok);
    utassert(num == 8);
    // Cursor on the wrapped "18" on line 2 (idx 15 → x = 72, y = 220).
    num = 0;
    ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{75, 224}, &num);
    utassert(ok);
    utassert(num == 18);
    // Cursor on "17" on line 1 (idx 12 → x = 144).
    num = 0;
    ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{147, 206}, &num);
    utassert(ok);
    utassert(num == 17);
}

// (13e) Wrapped citation where the open bracket sits at the END of line 1
// (high x) and the wrapped numbers are at the START of line 2 (low x) —
// "...[42,\n43] maintain...". Mirrors the real "[42, 43]" failure: detection
// walks by reading order (not x), so hovering "43" on line 2 must still find
// the '[' on line 1 and resolve to 43.
static void NumericCitationWrapEndOfLine() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    // Line 1 is a full column line ending in "[42," (the citation wrapped at
    // the right margin); line 2 begins with "43]" at the left margin. The two
    // share the column's x-range even though the citation halves do not.
    AddText(text, coords, n, 256, L"a first line of text ending in [42,", 72, 200);
    AddText(text, coords, n, 256, L"43] maintain live text here", 72, 224);
    // '4' of the wrapped "43" is the first glyph of the 2nd line.
    int idx43 = 0;
    for (int i = 0; i < n; i++) {
        if (text[i] == L'4' && coords[i].y == 224 && coords[i].x == 72) {
            idx43 = i;
            break;
        }
    }
    utassert(idx43 > 0);
    int num = 0;
    // Cursor on the wrapped "43" (x=72, y=224).
    bool ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{75, 230}, &num);
    utassert(ok);
    utassert(num == 43);
    // Cursor on "42" at the end of line 1.
    int idx42 = 0;
    for (int i = 0; i < n; i++) {
        if (text[i] == L'4' && coords[i].y == 200 && text[i + 1] == L'2') {
            idx42 = i;
            break;
        }
    }
    utassert(idx42 > 0);
    num = 0;
    ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{coords[idx42].x + 3, 206}, &num);
    utassert(ok);
    utassert(num == 42);
}

// (13f) Wrapped citation whose glyphs are OUT OF READING ORDER in the array:
// "[42," (line 1) and "43]" (line 2) are visually one line apart, but in the
// glyph stream "43]" is preceded by unrelated text ~200pt above (mirrors the
// real PDF: the stream neighbour of "43]" sat 196pt up, not at "[42,"). The
// detector must reconstruct local reading order spatially and still resolve.
static void NumericCitationWrapOutOfOrder() {
    WCHAR text[512];
    Rect coords[512];
    int n = 0;
    // line 1 (y=200): "...) [42," — the '[' sits near the right end.
    AddText(text, coords, n, 512, L"runtime models [42,", 72, 200);
    // Unrelated text ~200pt above, inserted BEFORE line 2 in array order so it
    // becomes "43]"'s stream neighbour (as in the real doc).
    AddText(text, coords, n, 512, L"unrelated heading text far above", 72, 4);
    // line 2 (y=220): "43] maintain ..." — visually one line below line 1.
    AddText(text, coords, n, 512, L"43] maintain live", 72, 220);
    // '4' of "43" is the first glyph of the 3rd AddText run.
    int idx43 = 0;
    for (int i = 0; i < n; i++) {
        if (text[i] == L'4' && coords[i].y == 220 && coords[i].x == 72) {
            idx43 = i;
            break;
        }
    }
    utassert(idx43 > 0);
    int num = 0;
    // Cursor on the wrapped "43" (x=72, y=220).
    bool ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{75, 226}, &num);
    utassert(ok);
    utassert(num == 43);
}

// (13g) Wrapped citation in the LEFT column of a 2-column page, with unrelated
// right-column text at the same y. Column-limited segment reconstruction must
// keep the right column out and still bridge "[42,"(line1) → "43]"(line2).
static void NumericCitationWrapTwoColumn() {
    WCHAR text[512];
    Rect coords[512];
    int n = 0;
    // Left column (x=72, ends ~x=270). Line 1 ends "[42,", line 2 starts "43]".
    AddText(text, coords, n, 512, L"left column line one ending [42,", 72, 200);
    AddText(text, coords, n, 512, L"43] left column line two here", 72, 220);
    // Right column (x=340) at the same two y's — must be ignored (gutter ~70pt).
    AddText(text, coords, n, 512, L"right column first line text", 340, 200);
    AddText(text, coords, n, 512, L"right column second line text", 340, 220);
    // '4' of the wrapped "43" (left column, x=72, y=220).
    int idx43 = 0;
    for (int i = 0; i < n; i++) {
        if (text[i] == L'4' && coords[i].x == 72 && coords[i].y == 220) {
            idx43 = i;
            break;
        }
    }
    utassert(idx43 > 0);
    int num = 0;
    bool ok = DetectNumericCitationInPageText(WStr(text, n), coords, n, Point{75, 226}, &num);
    utassert(ok);
    utassert(num == 43);
}

// (10b) Two author-year citations on one line get distinct srcRect x spans.
static void PlainTextCitationSrcRectDistinctOnLine() {
    WCHAR text[256];
    Rect coords[256];
    int n = 0;
    AddText(text, coords, n, 256, L"per (Smith, 2020) and (Jones, 2021) here", 72, 200);
    Str surname{};
    int year = 0;
    Rect first{}, second{};
    // Cursor on 'S' of Smith (glyph 6 → x = 72 + 6*6 = 108).
    bool ok = DetectCitationInPageText(WStr(text, n), coords, n, Point{110, 206}, &surname, &year, &first);
    utassert(ok);
    utassert(surname && str::Eq(surname, "Smith"));
    utassert(year == 2020);
    utassert(first.dx > 0);
    str::Free(surname);
    // Cursor on 'J' of Jones (glyph 25 → x = 72 + 25*6 = 222).
    ok = DetectCitationInPageText(WStr(text, n), coords, n, Point{224, 206}, &surname, &year, &second);
    utassert(ok);
    utassert(surname && str::Eq(surname, "Jones"));
    utassert(year == 2021);
    utassert(second.dx > 0);
    str::Free(surname);
    utassert(first.y == second.y);
    utassert(first.x != second.x);
}

// (14) Numeric reference list lookup: a line starting with "[num]" at the left
// column anchors at its first glyph; a number not present returns false.
static void NumericReferenceFoundOnBibPage() {
    WCHAR text[512];
    Rect coords[512];
    int n = 0;
    AddText(text, coords, n, 512, L"References", 72, 100);
    AddText(text, coords, n, 512, L"[1] A. Trentin, Some title 2025.", 72, 130);
    AddText(text, coords, n, 512, L"[2] B. Other, Another title 2024.", 72, 150);
    float x = 0.f, y = 0.f;
    bool ok = FindNumericReferenceInPageText(WStr(text, n), coords, n, 1, &x, &y);
    utassert(ok);
    utassert(x == 72.f);
    utassert(y == 130.f);
    ok = FindNumericReferenceInPageText(WStr(text, n), coords, n, 2, &x, &y);
    utassert(ok);
    utassert(y == 150.f);
    ok = FindNumericReferenceInPageText(WStr(text, n), coords, n, 3, &x, &y);
    utassert(!ok);
}

// (3g) Hanging-indent bracket bibliography with a narrow label ("[TA05]"):
// biblatex sizes the label column for the widest label, so a narrow label is
// separated from its body by a labelsep gap wider than the within-line gap
// threshold. The detected box must still span the full body width (the body
// is bridged), not collapse to the label width and clip the entry.
static void HangingIndentNarrowLabelFullWidth() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Narrow label "[TA05]" ends at x=72+6*6=108; body starts at x=130 — a
    // 22pt labelsep gap, wider than LineRunExtent's 20pt within-line gap.
    AddText(text, coords, n, 1024, L"[TA05]", 72, 200);
    AddText(text, coords, n, 1024, L"J. Tyree and A. Akerman. Architecture decisions.", 130, 200);
    AddText(text, coords, n, 1024, L"continuation line two of the same entry.", 130, 215);
    AddText(text, coords, n, 1024, L"[Vai20]", 72, 240);
    AddText(text, coords, n, 1024, L"Thomas Vaillant. Log4brains. 2020.", 130, 240);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // box must reach across the body (without the labelsep bridge it would
    // collapse to ~the label width, near x=150)
    utassert(box.x + box.dx >= 300.f);
    // still bounded to the first entry
    utassert(box.y + box.dy < 240.f);
    utassert(box.x <= 72.f + 6.f);
}

// (3h) 2-column layout where the left-column entry has a wide labelsep gap:
// the body bridge must reach the body without jumping the (much wider) column
// gutter into the right column.
static void TwoColumnHangingIndentStaysInColumn() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Left column: narrow label at x=72 (ends ~108), body at x=130 (22pt
    // labelsep), body line ends ~246.
    AddText(text, coords, n, 1024, L"[TA05]", 72, 200);
    AddText(text, coords, n, 1024, L"Smith J. 2010. Some title.", 130, 200);
    AddText(text, coords, n, 1024, L"continuation of the entry.", 130, 215);
    AddText(text, coords, n, 1024, L"[Vai20]", 72, 240);
    // Right column body at x=340 (gutter ~286..340), same y range. The body
    // bridge (capped at 50pt) cannot reach x=340 from the label run, and the
    // dense left-column body run stops at the gutter.
    for (int i = 0; i < 4; i++) {
        AddText(text, coords, n, 1024, L"right column body text line..", 340, 200 + i * 15);
    }
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    // box must not reach into the right column at x=340
    utassert(box.x + box.dx < 340.f);
    utassert(box.y + box.dy < 240.f);
}

// (3i) Last entry on a bibliography page (no sibling "[" below) followed by a
// page-number footer far below: the trailing-gap trim ends the box at the
// entry's last line, not at the footer.
static void LastEntryTrailingFooterTrimmed() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    AddText(text, coords, n, 1024, L"[Zim25]", 72, 600);
    AddText(text, coords, n, 1024, L"Olaf Zimmermann. ADG: A Light Tool.", 130, 600);
    AddText(text, coords, n, 1024, L"continuation of the last entry.", 130, 615);
    // page-number footer far below (73pt gap), centered in the text column
    AddText(text, coords, n, 1024, L"13", 300, 700);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 600.f);
    utassert(!IsEmpty(box));
    // footer at y=700 must be excluded (box ends just below the 2nd line)
    utassert(box.y + box.dy < 650.f);
}

// (12) NormalizeGlyphLines flattens a line whose glyphs have varying tops
// (mupdf's tight ink boxes) to a single top-aligned row, keyed by baseline,
// while keeping distinct lines separate.
static void NormalizeGlyphLinesFlattensLine() {
    // Line 1 at baseline 110: a tall glyph (top 100, h 10) and a low one
    // (a period: top 105, h 5) — same baseline. Line 2 at baseline 130.
    Rect in[3] = {Rect{72, 100, 6, 10}, Rect{78, 105, 4, 5}, Rect{72, 120, 6, 10}};
    Rect out[3];
    NormalizeGlyphLines(in, out, 3);
    // line 1 glyphs flattened to the same top (100) and height (110-100=10)
    utassert(out[0].y == 100 && out[0].dy == 10);
    utassert(out[1].y == 100 && out[1].dy == 10);
    // line 2 stays distinct
    utassert(out[2].y == 120 && out[2].dy == 10);
}

// (13) mupdf-style variable glyph tops: the previous entry's trailing line
// ends with a period whose tight ink box top sits below the digits on the
// same line and inside the destination band. Without baseline normalization
// that period hijacks the entry-start search (narrow strip / wrong entry);
// after NormalizeGlyphLines the detector locks onto the real "[Buc+23]" entry.
static void VariableGlyphTopsEntryNotHijacked() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Previous entry's trailing line "1622292." at baseline 313: digits top
    // 305 (h 8), final period top 310 (h 3) — same baseline, body column x.
    WStr tail = L"1622292";
    for (int i = 0; i < tail.len; i++) {
        text[n] = tail.s[i];
        coords[n] = Rect{130 + i * 6, 305, 6, 8};
        n++;
    }
    text[n] = L'.';
    coords[n] = Rect{130 + 7 * 6, 310, 4, 3};
    n++;
    // Entry "[Buc+23]" label (x=72) + body (x=130) at baseline ~327 (top 316).
    WStr label = L"[Buc+23]";
    for (int i = 0; i < label.len; i++) {
        text[n] = label.s[i];
        coords[n] = Rect{72 + i * 6, 316, 6, 11};
        n++;
    }
    WStr body = L"Georg Buchgeher et al. Using ADRs in Open Source.";
    for (int i = 0; i < body.len; i++) {
        text[n] = body.s[i];
        coords[n] = Rect{130 + i * 6, 316, 6, 9};
        n++;
    }
    // Next entry "[JB05]" below at top 352.
    WStr nb = L"[JB05] A. Jansen and J. Bosch.";
    for (int i = 0; i < nb.len; i++) {
        text[n] = nb.s[i];
        coords[n] = Rect{72 + i * 6, 352, 6, 10};
        n++;
    }
    Rect norm[1024];
    NormalizeGlyphLines(coords, norm, n);
    RectF box = DetectEntryBox(WStr(text, n), norm, Mediabox(), 72.f, 312.f);
    utassert(!IsEmpty(box));
    // entry start, not the previous line (normalized top 305)
    utassert(box.y > 305.f);
    // label included and full body width captured (not a narrow strip)
    utassert(box.x <= 72.f + 6.f);
    utassert(box.x + box.dx >= 300.f);
    // ends before the next entry
    utassert(box.y + box.dy < 352.f);
}

// (14) 2-column numeric reference list, destination in the LEFT column whose
// "[N] body" sits on a single line (no labelsep gap). The right column has a
// line a few pt ABOVE the destination Y and starts just past a narrow gutter.
// The box must anchor on the left entry (not the higher right-column line) and
// must not bleed across the gutter into the right column. Regresses the
// "hover [2] shows [26]" bug: the start-glyph pick latched onto the topmost
// line in the Y-window (right column), and the labelsep bridge then jumped the
// narrow gutter, widening the box across both columns.
static void TwoColumnNumericLeftEntryNotHijacked() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Left column (x=72). The "[2]" entry spans one full line (~ends x=198).
    AddText(text, coords, n, 1024, L"[1] First left column entry.", 72, 600);
    AddText(text, coords, n, 1024, L"[2] Anvaari Zimmerman", 72, 628);
    AddText(text, coords, n, 1024, L"second line of entry two", 72, 642);
    // Right column (x=244 — a 46pt gutter from the left column's ~198 edge,
    // narrower than the labelsep bridge's 50pt reach). Its first line at y=624
    // sits 4pt above the [2] destination top (628).
    AddText(text, coords, n, 1024, L"[26] Zimmermann Miksovic Decisions", 244, 624);
    AddText(text, coords, n, 1024, L"connecting enterprise architects", 244, 638);
    AddText(text, coords, n, 1024, L"[27] Zimmermann Zdun Combining", 244, 660);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 628.f);
    utassert(!IsEmpty(box));
    // anchored at the left-column entry, not the higher right-column line
    utassert(box.x <= 72.f + 6.f);
    // includes the [2] line
    utassert(box.y <= 628.f + 2.f);
    // must not cross the gutter into the right column (x=244)
    utassert(box.x + box.dx < 244.f);
    // bounded to the [2] entry's two lines (last line bottom ~654 + padding),
    // not sweeping down into the right column's [27] row
    utassert(box.y + box.dy < 672.f);
}

// (15) 2-column numeric reference list lookup: an entry in the RIGHT column
// must be found. Regresses the bug where the single-leftmost-column test (and
// the reading-order line-start test) made every right-column entry unreachable
// — hovering "[4]"/"[5]" then did nothing because their entries sit in the
// right column of the references page.
static void TwoColumnNumericReferenceFound() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Left column refs at x=72, right column refs at x=320 (a wide gutter from
    // the left column's text which ends near x=150).
    AddText(text, coords, n, 1024, L"[1] Left column one.", 72, 100);
    AddText(text, coords, n, 1024, L"[2] Left column two.", 72, 120);
    AddText(text, coords, n, 1024, L"[3] Right column three.", 320, 100);
    AddText(text, coords, n, 1024, L"[4] Right column four.", 320, 120);
    float x = 0, y = 0;
    // right-column entries must resolve
    utassert(FindNumericReferenceInPageText(WStr(text, n), coords, n, 4, &x, &y));
    utassert(x == 320.f && y == 120.f);
    utassert(FindNumericReferenceInPageText(WStr(text, n), coords, n, 3, &x, &y));
    utassert(x == 320.f && y == 100.f);
    // left-column entries still resolve
    utassert(FindNumericReferenceInPageText(WStr(text, n), coords, n, 1, &x, &y));
    utassert(x == 72.f && y == 100.f);
    // absent number fails
    utassert(!FindNumericReferenceInPageText(WStr(text, n), coords, n, 9, &x, &y));
    // a mid-line body citation "[2]" (text to its left) is not an entry start
    n = 0;
    AddText(text, coords, n, 1024, L"as reported in [2] by others", 72, 200);
    utassert(!FindNumericReferenceInPageText(WStr(text, n), coords, n, 2, &x, &y));
}

// (16) 2-column entry whose second line is much wider than its first (e.g. a
// short label line over a long URL). The box must cover the wide line's full
// width yet stop at the column gutter. Regresses two failure modes: sizing the
// box from line 1 clips the wide line; a gap-threshold walk crosses the narrow
// gutter into the neighbouring column. The gutter-aware column scan handles
// both (the gutter is empty on every row; the wide line keeps its column full).
static void TwoColumnWideSecondLineStaysInColumn() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Left column entry at x=72: short first line (~ends x=168), long 2nd line
    // (~ends x=276). Right column at x=300 — a ~24pt gutter.
    AddText(text, coords, n, 1024, L"[5] Short label.", 72, 200);
    AddText(text, coords, n, 1024, L"http://example.com/a/very/long/url", 72, 214);
    AddText(text, coords, n, 1024, L"[9] Right one.", 300, 200);
    AddText(text, coords, n, 1024, L"right two.", 300, 214);
    RectF box = DetectEntryBox(WStr(text, n), coords, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    utassert(box.x <= 72.f + 6.f);
    // covers the long 2nd line (well past the short first line's ~x=168)
    utassert(box.x + box.dx >= 250.f);
    // but does not cross the gutter into the right column (x=300)
    utassert(box.x + box.dx < 300.f);
}

// (17) StripWatermarkGlyphs: a diagonal draft / "under review" watermark
// (oversized glyphs, one per baseline) is removed, while body text and a
// horizontal heading of same-baseline tall glyphs are kept.
static void StripWatermarkRemovesDiagonalStamp() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Body: 3 lines of normal (dy=12) glyphs.
    AddText(text, coords, n, 1024, L"normal body text line one here", 72, 200);
    AddText(text, coords, n, 1024, L"normal body text line two here", 72, 214);
    AddText(text, coords, n, 1024, L"normal body text line three xx", 72, 228);
    int bodyGlyphs = n;
    // Diagonal watermark: oversized (dy=40), each glyph on its own baseline.
    for (int i = 0; i < 10 && n < 1024; i++) {
        text[n] = L"UNDERREVIEW"[i];
        coords[n] = Rect{190 + i * 14, 180 + i * 6, 20, 40};
        n++;
    }
    WCHAR outText[1024];
    Rect outCoords[1024];
    int kept = StripWatermarkGlyphs(WStr(text, n), coords, outText, outCoords);
    // All body glyphs survive; all 10 watermark glyphs are dropped.
    utassert(kept == bodyGlyphs);
    for (int i = 0; i < kept; i++) {
        utassert(outCoords[i].dy == kLineH);
    }

    // A horizontal heading of tall glyphs (same baseline, many on the row) is
    // NOT mistaken for a watermark — tall + dense row ⇒ kept.
    n = 0;
    AddText(text, coords, n, 1024, L"normal body text line one here", 72, 200);
    int headStart = n;
    for (int i = 0; i < 8 && n < 1024; i++) {
        text[n] = L"HEADING!"[i];
        coords[n] = Rect{72 + i * 14, 150, 12, 30}; // tall, but all share baseline 180
        n++;
    }
    kept = StripWatermarkGlyphs(WStr(text, n), coords, outText, outCoords);
    utassert(kept == n); // nothing stripped
    (void)headStart;
}

// (18) 2-column reference list overlaid by a diagonal draft / "under review"
// watermark whose oversized glyphs pass through the column gutter. The caller
// strips the watermark (StripWatermarkGlyphs) before box detection, so the
// gutter is empty again and the box stays in the left column. Regresses the
// "ref spans two columns" bug: the watermark filled the gutter in the column
// scan, columnRightX ran past it, and the box swept into the right column.
static void TwoColumnWatermarkStaysInColumn() {
    WCHAR text[1024];
    Rect coords[1024];
    int n = 0;
    // Left column entry at x=72 (label+body share line 1, ends ~x=246), 2nd
    // line at y=215. Right column body at x=340 (gutter ~246..340).
    AddText(text, coords, n, 1024, L"[12] Sample left column entry", 72, 200);
    AddText(text, coords, n, 1024, L"more left column entry text..", 72, 215);
    for (int i = 0; i < 4; i++) {
        AddText(text, coords, n, 1024, L"right column body text line..", 340, 200 + i * 15);
    }
    // Diagonal watermark: oversized (dy=40), wide (dx=20) glyphs stepping right
    // by 14pt and down by 6pt, sweeping x≈190..360 across the gutter.
    for (int i = 0; i < 12 && n < 1024; i++) {
        text[n] = L"UNDERREVIEW.."[i];
        coords[n] = Rect{190 + i * 14, 180 + i * 6, 20, 40};
        n++;
    }
    // Caller pipeline: strip the watermark, then detect on the survivors.
    WCHAR cleanText[1024];
    Rect cleanCoords[1024];
    int cleanLen = StripWatermarkGlyphs(WStr(text, n), coords, cleanText, cleanCoords);
    RectF box = DetectEntryBox(WStr(cleanText, cleanLen), cleanCoords, Mediabox(), 72.f, 200.f);
    utassert(!IsEmpty(box));
    utassert(box.x <= 72.f + 6.f);
    // must not cross the gutter into the right column (x=340)
    utassert(box.x + box.dx < 340.f);
}

void RefHoverTest() {
    TwoColumnNumericLeftEntryNotHijacked();
    TwoColumnNumericReferenceFound();
    TwoColumnWideSecondLineStaysInColumn();
    StripWatermarkRemovesDiagonalStamp();
    TwoColumnWatermarkStaysInColumn();
    AccentedAllCapsHeadingDetected();
    HangingIndentNarrowLabelFullWidth();
    TwoColumnHangingIndentStaysInColumn();
    LastEntryTrailingFooterTrimmed();
    NormalizeGlyphLinesFlattensLine();
    VariableGlyphTopsEntryNotHijacked();
    AuthorYearEntryFitsToOneEntry();
    BodyTextParenRejected();
    BracketEntryFitsToOneEntry();
    CaptionExtensionPicksTopmost();
    GapSeparatedEntryList();
    SingleLineEntryList();
    EmptyInputsHandled();
    EquationLabelDetected();
    BibYearParenNotEquationLabel();
    FrenchCaptionDetected();
    ItalianPortugueseCaptionDetected();
    LandscapeBoxBasicShape();
    NegativeDestYFallsToLandscape();
    NonTrailingParenRejected();
    PluralHeadingWordNotMatched();
    SparseTextReturnsWholePage();
    TwoColumnLeftEntryStaysInColumn();
    TwoColumnRightEntryStaysInColumn();
    ColumnWrapContinuationDetected();
    ColumnWrapContinuationRejectsUnrelatedBodyText();
    PlainTextCitationDetected();
    PlainTextCitationSrcRectDistinctOnLine();
    PlainTextCitationNoYear();
    SurnameFoundOnBibPage();
    NumericCitationDetected();
    NumericCitationSrcRectDistinctOnLine();
    NumericCitationRangeEnDash();
    NumericCitationLineBreakList();
    NumericCitationWrapEndOfLine();
    NumericCitationWrapOutOfOrder();
    NumericCitationWrapTwoColumn();
    NumericReferenceFoundOnBibPage();
}
