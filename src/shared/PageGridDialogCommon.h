/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by PageGridDialogCommon.cpp and each app's PageGridDialog.cpp ---

// implemented by each app
bool ShowTransparencyGrid();
bool ShowPageGrid();

extern SeqStrings kPageGridUnitTok;
extern SeqStrings kPageGridStyleTok;
constexpr float kPageGridDefaultSizePt = 72.f;
constexpr int kPageGridDefaultSubdivisions = 4;
constexpr Color kPageGridDefaultColor = MkRgb(128, 128, 255);
constexpr int kPageGridDefaultStyleIdx = 0;
constexpr int kPageGridDefaultUnitIdx = 1;
struct PageGridSnap {
    float width = kPageGridDefaultSizePt;
    float height = kPageGridDefaultSizePt;
    int subdivisions = kPageGridDefaultSubdivisions;
    float offsetX = 0;
    float offsetY = 0;
    Str color;
    Str style;
    Str units;
    bool showGrid = false;
};
float PageGridToPt(float v, int unit);
float PageGridFromPt(float pt, int unit);
TempStr PageGridNumTemp(float v);
Str PageGridUnitName(int i);
Str PageGridStyleName(int i);
PageGrid* PageGridPrefs();
void CopyPageGridSnap(PageGridSnap& dst, const PageGrid& src, bool showGrid);
