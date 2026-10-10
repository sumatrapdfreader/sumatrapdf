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
