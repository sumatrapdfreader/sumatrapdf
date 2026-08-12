/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct Pixmap;
enum class TbIcon;

// A toolbar icon rendered into a Pixmap of the given size, in the current
// theme's colors. Cached: the Pixmap belongs to the cache and stays valid
// until DestroyIconPixmaps(), which the theme change and the shutdown call
Pixmap* GetPixmapForIcon(TbIcon, int dx, int dy);
void DestroyIconPixmaps();

void CreateToolbar(MainWindow*);
void ReCreateToolbar(MainWindow* win);
void ToolbarUpdateStateForWindow(MainWindow*, bool setButtonsVisibility);
void UpdateToolbarButtonsToolTipsForWindow(MainWindow*);
void UpdateToolbarFindText(MainWindow*);
void UpdateToolbarPageText(MainWindow*, int pageCount, bool updateOnly = false);
void UpdateFindbox(MainWindow*);
void SetToolbarButtonEnableState(MainWindow*, int cmdId, bool isEnabled);
void SetToolbarButtonCheckedState(MainWindow*, int cmdId, bool isChecked);
bool ShouldShowToolbar(MainWindow*);
bool ShouldOverlayToolbar(MainWindow*);
void ShowOrHideToolbar(MainWindow*);
void PositionOverlayToolbar(MainWindow*);
void UpdateOverlayToolbarForMouse(MainWindow*);
void RevealOverlayToolbar(MainWindow*);
void OverlayToolbarHideTimerFired(MainWindow*);

// delay before the overlay toolbar hides after the mouse moves away
constexpr int kDelayToolbarHide = 500;
#define kHideOverlayToolbarTimerId 0x101
void UpdateToolbarState(MainWindow*);
void UpdateToolbarAfterThemeChange(MainWindow*);
// renders an svg icon into a dx by dy Pixmap whose background is transparent,
// for code that draws its own buttons (the selection toolbar). Caller owns the
// Pixmap; null if the svg couldn't be rendered
Pixmap* RenderSvgIconToPixmap(Str svgData, int dx, int dy, COLORREF fgCol, COLORREF bgCol);
Rect GetToolbarButtonScreenRect(MainWindow*, int cmdId);

TempStr ToolbarButtonsResultTemp(int* exitCodeOut);

int GetMenuBarRebarHeight(MainWindow*);
void CreateMenuBarRebar(MainWindow*);
void DestroyMenuBarRebar(MainWindow*);
void ShowMenuBarRebar(MainWindow*);
void RebuildMenuBarButtons(MainWindow*);
bool IsShowingMenuBarRebar(MainWindow*);
bool HandleMenuBarCommand(MainWindow*, int cmdId);
bool ActivateMenuBarByAccel(MainWindow*, WCHAR accel);
void UpdateCustomMenuBarMenuSelect(MainWindow*, WPARAM, LPARAM);
