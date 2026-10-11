/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by ToolbarCommon.cpp and each app's Toolbar.cpp ---

struct ToolbarButtonInfo {
    const char* icon = nullptr; // gIcon*, or null for a separator / page box / text
    int cmdId = 0;
    Str toolTip;
    Str svgIcon; // custom SVG from settings
    bool isText = false;
};
constexpr int kMaxCustomButtons = 127;
constexpr int kMaxLayoutButtons = 64;

// those are not real commands but we have to refer to toolbar buttons
// is by a command. those are just background for area to be
// covered by other HWNDs. They need the right size
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;

extern int gLayoutButtonsCount;
extern ToolbarButtonInfo gCustomButtons[kMaxCustomButtons + 1];
Color TbBgColor();
Color TbTextColor();
Color TbDisabledColor();
Color TbHoverColor();
Color TbSubtleBgColor();
Color TbSelectedColor();
Color TbEdgeColor();
int ToolbarCyPad();
int ToolbarRowDy(int iconSize);
bool HasToolbarButtonContent(const ToolbarButtonInfo& tbi);
int TotalButtonsCount();
int OriginalCommandId(int cmdId);
TempStr ToolbarTipTemp(int cmdId, Str tip, bool translate);
int ToolbarModeForWindow(MainWindow* win);
void PopulateCustomToolbarButtons();
int HoverPyramidTopRow(int n);
struct ZoomHoverLevel {
    float zoom;
    int cmdId;
};
void ZoomHoverLevels(Vec<ZoomHoverLevel>& out);
int ZoomHoverCurrentIdx(MainWindow* win, const Vec<ZoomHoverLevel>& levels);
ParsedColor* AnnotPresetColorSetting(int cmdId);
Color AnnotDefaultColor(int cmdId);
Str* AnnotPresetColorList(int cmdId);
void SetAnnotPresetColor(int cmdId, Color col);
bool CanCreateAnnotFromSelection(MainWindow* win, int cmdId);
bool SameColorAndAlpha(Color a, Color b);

extern ToolbarButtonInfo gLayoutButtons[kMaxLayoutButtons];
extern bool gLayoutParsed;
void PopulateToolbarLayout();
ToolbarButtonInfo& GetToolbarButtonInfoByIdx(int idx);
void EnsureAnnotPresetColor(int cmdId, Color col);

// implemented by each app
void SetPdfAnnotationsToolbarEnabled(MainWindow* win, bool enabled);

// the "Edit PDF" row under the toolbar
extern ToolbarButtonInfo gPdfAnnotationButtons[];
extern const int kPdfAnnotationButtonsCount;

bool IsCmdAvailable(MainWindow* win, int cmdId, AppCommandCtx* ctx);

bool IsCmdEnabled(MainWindow* win, int cmdId, AppCommandCtx* ctx);

constexpr int kButtonSpacingX = 4;

// distance between label and edit field
constexpr int kTextPaddingRight = 6;

// A row of NewToolbarHoverMenu(): an icon on the left, text on the right, and a
// background that lights up under the mouse, like a menu item.
constexpr int kHoverRowPadY = 6;

constexpr int kHoverRowPadX = 10;

constexpr int kHoverRowIconGapX = 8;

// between the label and the shortcut that sits at the right edge, as in a menu
constexpr int kHoverRowShortcutGapX = 24;

// around a label in the single-row strip; less than a menu row's, it is a row
// of them and the gaps add up
constexpr int kHoverCellPadX = 8;

// the mouse crosses a seam going from the button to the drop-down; don't close
// on the frame where it is over neither
constexpr int kCloseHoverDropdownDelayMs = 150;

// ring space around the circle, where the mark on the color in use goes
constexpr int kAnnotSwatchPad = 5;

// Annotations.InkBorderWidth is in PDF points, which is about a pixel at 100%
constexpr int kInkThicknessMin = 1;

constexpr int kInkThicknessMax = 16;

// which button's color the generic color dialog is editing; a valid onPick
// instead means it was opened for something that is not a toolbar button
struct AnnotColorsTarget {
    int cmdId = 0;
    Func1<Color> onPick;
};

void AnnotColorsPicked(AnnotColorsTarget* target, ChangeColorsArgs* args);
