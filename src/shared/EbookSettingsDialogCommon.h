/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by EbookSettingsDialogCommon.cpp and each app's EbookSettingsDialog.cpp ---

Str FontDefaultLabel();
void ParseMargin(Str s, Vec<float>& out);
TempStr MarginTextTemp(const Vec<float>* margin);
bool MarginEq(const Vec<float>& a, const Vec<float>* b);

// the page an ebook is laid out into (EBookUI.LayoutDx / LayoutDy), in the
// order the Page drop-down lists them
enum class EbookPageSize {
    FitWindow,
    A5,
    A4,
    Letter,
    Custom
};
void EbookPageSizeLabels(StrVec& out);
EbookPageSize EbookPageSizeFor(float dx, float dy);
void EbookPageSizeDims(EbookPageSize size, float* dx, float* dy);
float ParseLayoutPt(Str s);
TempStr LayoutPtTextTemp(float pt);

// what the controls hold, ready to be written to a settings struct
struct EbookVals {
    Str fontName; // not owned, points into the controls' temp strings
    float fontSize = 0;
    Vec<float> margin; // 1, 2 or 4 values; empty means unset
    float lineSpacing = 0;
    bool ignoreDocumentCSS = false;
    Str customCSS; // not owned; empty unless useCustomCSS
    bool useCustomCSS = false;
    // the page size in points; 0: the default width / the window's shape
    float layoutDx = 0;
    float layoutDy = 0;
};
