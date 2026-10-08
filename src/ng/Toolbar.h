/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Toolbar.cpp owns the toolbar: which buttons exist and in which order, whether
// they are available / enabled / checked, their tooltips and icons, and when the
// overlay toolbar shows and hides.
//
// ng: orig builds a tree of virtual controls in a window of its own and pokes
// at it (SetToolbarButtonHiddenByIdx, SetToolbarButtonImageByIdx, ...). Here
// ToolbarBuild() makes the row out of gpui elements every frame, so the
// orig functions that set one button's state become "recompute and repaint".
// The annotation row (orig's gPdfAnnotationButtons) is step 13.

namespace gpui {
struct Ctx;
struct El;
struct InputState;
} // namespace gpui

struct MainWindow;
// the gpui view entity of the toolbar; defined in Toolbar.cpp because this
// header is included by files that don't have gpui.h
struct ToolbarUI;

// delay before the overlay toolbar hides after the mouse moves away
constexpr int kDelayToolbarHide = 500;
// the mouse has to rest this long on a button before its drop-down opens
constexpr int kOpenHoverDropdownDelay = 500;

// ng: per-window toolbar state (orig's ToolbarVirt plus the overlay flags it
// keeps on MainWindow)
struct Toolbar {
    ToolbarUI* ui = nullptr;
    int iconSize = 0;
    int rowDy = 0;

    // the page / chapter boxes; gpui text fields, orig's win32 Edits
    gpui::InputState* pageEdit = nullptr;
    gpui::InputState* chapterEdit = nullptr;
    // what UpdateToolbarPageText computed: the " / N" after the page box, the
    // " / N" after the chapter box and whether the chapter box shows at all
    Str pageTotal;
    Str chapterTotal;
    bool hasChapters = false;
    // the page box should take the focus on the next frame (Ctrl+G), or select
    // all of its text (orig creates it with selectAllOnFocus)
    bool wantPageFocus = false;
    gpui::InputState* wantSelectAll = nullptr;

    // overlay mode: whether the floating bar is up and the hide countdown
    bool overlayShown = false;
    int overlayHideLeftMs = 0;

    // the hover drop-down: the button it is up for, the one the mouse is
    // resting on and how long is left before it opens
    int hoverCmdId = 0;
    int hoverPendingCmdId = 0;
    int hoverOpenLeftMs = 0;
    // > 0 while the open drop-down waits out orig's close grace
    int hoverCloseLeftMs = 0;

    ~Toolbar();
};

// orig's "Edit PDF" mode: the annotation button row under the toolbar
void TogglePdfAnnotationsToolbar(MainWindow*);
void EnablePdfAnnotationsToolbar(MainWindow*);
// the colors an annotation drop-down offers (Annotations.PresetColors, or
// Annotations.InkColors for the ink button); cmdId 0 gets the presets
void AnnotPresetColors(int cmdId, Vec<Color>& out);
Color AnnotColorForCmd(int cmdId);
void SetAnnotPresetColor(int cmdId, Color col);

gpui::El* ToolbarBuild(MainWindow*, gpui::Ctx*);
// what floats over the canvas: the zoom drop-down a zoom button opens
gpui::El* ToolbarOverlayBuild(MainWindow*, gpui::Ctx*);

void CreateToolbar(MainWindow*);
void ReCreateToolbar(MainWindow*);
void DestroyToolbar(MainWindow*);
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
void UpdateOverlayToolbarForMouse(MainWindow*, Point ptInFrame);
void RevealOverlayToolbar(MainWindow*);
void UpdateToolbarState(MainWindow*);
void UpdateToolbarAfterThemeChange(MainWindow*);
int ToolbarIconSize();
// height the toolbar takes in the frame, 0 when it is hidden or floating
int ToolbarDy(MainWindow*);
// ng: orig's GetToolbarButtonScreenRect is in screen coordinates, for the
// popup windows it anchors; here everything is drawn in the frame
Rect GetToolbarButtonRect(MainWindow*, int cmdId);
TempStr ToolbarButtonsResultTemp(int* exitCodeOut);
// the overlay's hide countdown and the drop-down's open delay
void ToolbarTick(MainWindow*, int elapsedMs);
// Ctrl+G: focus the page box when the toolbar is up, else say so and let the
// caller open the Go To Page dialog
bool ToolbarFocusPageBox(MainWindow*);
bool IsToolbarPageBoxFocused(MainWindow*);
// the two boxes apart, for orig's AdvanceFocus
bool ToolbarHasChapterBox(MainWindow*);
bool IsToolbarLocationBoxFocused(MainWindow*, bool chapter);
void ToolbarFocusLocationBox(MainWindow*, bool chapter);
// Escape leaves the page box; the shell asks before it does anything else
bool ToolbarOnEscape(MainWindow*);
