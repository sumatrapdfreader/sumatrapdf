/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by PageGridDialogCommon.cpp and each app's PageGridDialog.cpp ---

struct DisplayModel;

// implemented by each app
bool ShowTransparencyGrid();
bool ShowPageGrid();
bool IsPageGridDialogOpen();
void PageGridDialogSyncFromPrefs();

extern SeqStrings kPageGridUnitTok;
extern SeqStrings kPageGridStyleTok;
constexpr float kPageGridDefaultSizePt = 72.f;
constexpr int kPageGridDefaultSubdivisions = 4;
constexpr Color kPageGridDefaultColor = MkRgb(128, 128, 255);
constexpr int kPageGridDefaultStyleIdx = 0;
constexpr int kPageGridDefaultUnitIdx = 1;
// Degrees from the horizontal. 90 is upright; 52–60 is a typical script slant.
constexpr float kPageGridDefaultSlant = 90.f;
constexpr float kPageGridMinSlant = 15.f;
constexpr float kPageGridMaxSlant = 165.f;
struct PageGridSnap {
    float width = kPageGridDefaultSizePt;
    float height = kPageGridDefaultSizePt;
    int subdivisions = kPageGridDefaultSubdivisions;
    float offsetX = 0;
    float offsetY = 0;
    float slant = kPageGridDefaultSlant;
    Str color;
    Str style;
    Str units;
    bool showGrid = false;
};

enum class PageGridStyleKind {
    Dots,
    Dotted,
    Solid
};

struct PageGridDraw {
    float widthPt = kPageGridDefaultSizePt;
    float heightPt = kPageGridDefaultSizePt;
    int subdiv = kPageGridDefaultSubdivisions;
    float offsetXPt = 0;
    float offsetYPt = 0;
    float slantDeg = kPageGridDefaultSlant;
    Color color = kPageGridDefaultColor;
    PageGridStyleKind style = PageGridStyleKind::Dots;
};

// One grid mark in page space. A dot has dot set and x0,y0 only.
struct PageGridSeg {
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
    bool major = false;
    bool dot = false;
};

enum class PageGridAxis {
    None,
    Width,
    Height
};

float PageGridToPt(float v, int unit);
float PageGridFromPt(float pt, int unit);
TempStr PageGridNumTemp(float v);
Str PageGridUnitName(int i);
Str PageGridStyleName(int i);
PageGrid* PageGridPrefs();
void CopyPageGridSnap(PageGridSnap& dst, const PageGrid& src, bool showGrid);
void RedrawPageGridWindows();
float PageGridAlignDown(float v, float origin, float step);
bool PageGridIsMajor(float v, float origin, float minorPt, int subdiv);
float PageGridClampSlant(float deg);
bool PageGridIsUpright(float deg);
PageGridDraw GetPageGridDraw();
void PageGridSlantedSegs(const PageGridDraw& g, const RectF& box, const RectF& vis, bool wantMinor,
                         Vec<PageGridSeg>& out);
bool PageGridSlantVisible(DisplayModel* dm, int pageNo, const PageGridDraw& g, const RectF& box, bool& drawMinor);
PageGridAxis PageGridCursorAxis(DisplayModel* dm, int x, int y, bool allow);
bool PageGridDragStart(DisplayModel* dm, int x, int y);
void PageGridDragMove(DisplayModel* dm, int x, int y);
bool PageGridDragActive();
void PageGridDragEnd();
void PageGridDragCancel();
TempStr PageGridProbeTemp(DisplayModel* dm);
