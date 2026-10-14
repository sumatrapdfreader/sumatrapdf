/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's ChangeThemeDialog.cpp is a WS_POPUPWINDOW placed beside the main
// window so the page stays visible while the theme is previewed. On wasm this
// is a gpui Dialog with the same rows: the theme list
// ("Follow Windows" first), the "Document colors follow theme" label and its
// drop-down, and Cancel / Change. The 300 ms preview debounce is orig's, run
// off the shell's tick instead of a WM_TIMER.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "Theme.h"
#include "PdfDarkMode.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/Dpi.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "AppHelpersCommon.h"
#include "ChangeThemeDialogCommon.h"

#include "SumatraLog.h"

static constexpr int kThemePreviewDebounceMs = 300;
static constexpr float kThemeListMinDy = 180;
static constexpr float kThemeListMaxDy = 16 * kDlgListRowDy;
static SeqStrings gDocumentColorsFollowThemeNames = "off\0smart\0legacy\0";

struct ChangeThemeDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    bool documentColorsFollowThemeOnly = false;
    int sel = -1;
    int previewMs = -1;
    Str startThemePref;
    DocumentColorsFollowTheme startDocumentColorsFollowTheme = DocumentColorsFollowTheme::Off;
    DialogSelect ddFollow;
    float scrollY = 0;
    // orig's window, where the platform can have one (gui/ToolWindow.h);
    // null: a dialog in the frame
    ToolWindow* tw = nullptr;
};

static ChangeThemeDlg gChangeTheme;

static void ChangeThemeOpenToolWindow(MainWindow* win);

struct ChangeThemeView {
    static void OnOk(ChangeThemeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(ChangeThemeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRowClick(ChangeThemeView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnScroll(ChangeThemeView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<ChangeThemeView> gChangeThemeView;

// orig's DocumentColorsFollowThemeToDropDownIndex / ...FromDropDownIndex
static int FollowThemeToIndex(DocumentColorsFollowTheme mode) {
    if (mode == DocumentColorsFollowTheme::Smart) {
        return 1;
    }
    if (mode == DocumentColorsFollowTheme::Legacy) {
        return 2;
    }
    return 0;
}

static DocumentColorsFollowTheme FollowThemeFromIndex(int idx) {
    if (idx == 1) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (idx == 2) {
        return DocumentColorsFollowTheme::Legacy;
    }
    return DocumentColorsFollowTheme::Off;
}

// the dialog in the frame; a window of its own is not the frame's business
bool IsChangeThemeDialogVisible() {
    return gChangeTheme.visible && !gChangeTheme.tw;
}

// orig's layout: the themes (16 rows at most), the drop-down's line, the
// footer, hairlines between
constexpr int kThemeDlgDx = 320;
constexpr int kThemeListRows = 16;
constexpr float kThemeComboDx = 90;

// the height of the list's view
static float ThemeListViewDy() {
    int n = ThemeGetCount() + 1;
    if (gChangeTheme.tw) {
        return (float)std::min(n, kThemeListRows) * kDlgListRowDy;
    }
    return DialogListViewDy(n, kDlgListRowDy, kThemeListMinDy, kThemeListMaxDy, 0);
}

// closes the window or the dialog without touching the settings
static void ChangeThemeHide() {
    gChangeTheme.visible = false;
    gChangeTheme.previewMs = -1;
    if (gChangeTheme.tw) {
        ToolWindowClose(gChangeTheme.tw);
        gChangeTheme.tw = nullptr;
    }
}

// orig's PreviewDocumentColors
static void PreviewDocumentColors() {
    UpdateDocumentColors();
    MainWindow* win = gChangeTheme.win;
    if (IsMainWindowValidAndNotClosing(win)) {
        win->RedrawAll(true);
    }
}

void CloseChangeThemeDialog() {
    if (!gChangeTheme.visible) {
        return;
    }
    ChangeThemeHide();
    // orig's OnCancel: put the theme and the document-colors mode back
    if (!gChangeTheme.documentColorsFollowThemeOnly) {
        SetTheme(gChangeTheme.startThemePref);
    }
    SetDocumentColorsFollowTheme(gChangeTheme.startDocumentColorsFollowTheme);
    SumatraUpdateTheme();
    PreviewDocumentColors();
    gChangeTheme.ddFollow.Free();
    str::Free(gChangeTheme.startThemePref);
    gChangeTheme.startThemePref = {};
    AppShellInvalidate(gChangeTheme.win);
}

// orig's ApplyPreview: live-preview the list's current theme
static void ApplyPreview() {
    gChangeTheme.previewMs = -1;
    int idx = gChangeTheme.sel;
    if (idx < 0) {
        return;
    }
    if (idx == kFollowWindowsThemeListIndex) {
        SetTheme(StrL("System"));
    } else {
        SetThemeByIndex(idx - 1);
    }
    SumatraUpdateTheme();
    PreviewDocumentColors();
}

// orig arrows through the list without rebuilding the chrome on every item
static void SchedulePreview() {
    gChangeTheme.previewMs = kThemePreviewDebounceMs;
}

void ChangeThemePreviewTick(int ms) {
    if (!gChangeTheme.visible || gChangeTheme.previewMs < 0) {
        return;
    }
    gChangeTheme.previewMs -= ms;
    if (gChangeTheme.previewMs > 0) {
        return;
    }
    ApplyPreview();
    AppShellInvalidate(gChangeTheme.win);
}

// orig's ChangeThemeWnd::OnChange
void ChangeThemeOk() {
    ApplyPreview();
    logf("ChangeThemeDialog: theme '%s', document colors follow theme %d\n", gSettings->theme,
         (int)GetDocumentColorsFollowTheme());
    ScheduleSaveSettings();
    // the settings the dialog applied are the ones to keep: close without the
    // revert Close...() does
    ChangeThemeHide();
    gChangeTheme.ddFollow.Free();
    str::Free(gChangeTheme.startThemePref);
    gChangeTheme.startThemePref = {};
    AppShellInvalidate(gChangeTheme.win);
}

// what orig's ListBox does when its selection changes
static void ScrollSelIntoView() {
    int n = ThemeGetCount() + 1;
    gChangeTheme.scrollY =
        DialogScrollToRow(gChangeTheme.scrollY, gChangeTheme.sel, n, kDlgListRowDy, ThemeListViewDy());
}

static void ThemeSelect(int sel) {
    gChangeTheme.sel = sel;
    ScrollSelIntoView();
    SchedulePreview();
    AppShellInvalidate(gChangeTheme.win);
}

static bool ThemeMoveSelection(int dir) {
    if (!gChangeTheme.visible || gChangeTheme.documentColorsFollowThemeOnly) {
        return false;
    }
    int n = ThemeGetCount() + 1;
    int sel = gChangeTheme.sel;
    sel = (sel < 0) ? (dir < 0 ? n - 1 : 0) : ((sel + dir + n) % n);
    ThemeSelect(sel);
    return true;
}

// the frame's Up / Down
bool ChangeThemeMoveSelection(int dir) {
    return !gChangeTheme.tw && ThemeMoveSelection(dir);
}

void ChangeThemeView::OnOk(ChangeThemeView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ChangeThemeOk();
    gp::Notify(cx);
}

void ChangeThemeView::OnCancel(ChangeThemeView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseChangeThemeDialog();
    gp::Notify(cx);
}

void ChangeThemeView::OnRowClick(ChangeThemeView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    gChangeTheme.sel = (int)idx;
    if (ev->clickCount >= 2) {
        ChangeThemeOk();
    } else {
        SchedulePreview();
    }
    gp::Notify(cx);
}

void ChangeThemeView::OnScroll(ChangeThemeView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gChangeTheme.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gChangeTheme.win);
}

// --- a window of its own (Windows) ------------------------------------------

static Str ChangeThemeToolTitle() {
    return gChangeTheme.documentColorsFollowThemeOnly ? Tr("Make Document Colors Follow Theme") : Tr("Change Theme");
}

static gp::El* ChangeThemeColumn(gp::Ctx* cx) {
    // orig's OnDocumentColorsFollowThemeChanged
    if (gChangeTheme.ddFollow.PollChanged(cx->app)) {
        SetDocumentColorsFollowTheme(FollowThemeFromIndex(gChangeTheme.ddFollow.sel));
        PreviewDocumentColors();
    }
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (!gChangeTheme.documentColorsFollowThemeOnly) {
        gp::El* list = gp::Div(cx->a)
                           ->Id(GStrL("theme-list"))
                           ->FlexCol()
                           ->W(gp::kFill)
                           ->H(ThemeListViewDy())
                           ->Shrink0()
                           ->ScrollY(gChangeTheme.scrollY)
                           ->ScrollFromPath()
                           ->OnScroll(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnScroll));
        int nThemes = ThemeGetCount();
        for (int i = 0; i <= nThemes; i++) {
            // orig: a mode, not another color theme, so its name is distinct
            // from the persisted Theme = System value
            Str name = (i == kFollowWindowsThemeListIndex) ? Tr("Follow Windows") : ThemeGetNameAt(i - 1);
            list->Child(DlgListRow(cx, fmt("theme-row-%d", i), name, i == gChangeTheme.sel,
                                   gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnRowClick, (intptr_t)i)));
        }
        col->Child(DlgListBox(cx, list));
        col->Child(DlgHairline(cx));
    }

    // whether the document's colors follow: the label muted, the drop-down
    Color colMuted = ThemeWindowTextDisabledColor();
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->W(gp::kFill)
                   ->H(kToolDlgLineDy)
                   ->Shrink0()
                   ->PadX(kToolDlgPadDx)
                   ->Gap(kToolDlgGapDx)
                   ->Child(gp::TextEl(cx->a, ToGpui(Tr("Document colors follow theme")))
                               ->Font(kToolDlgTextFontPx)
                               ->Fg(ToGpui(colMuted))
                               ->Truncate()
                               ->MinW(0))
                   ->Child(gp::Div(cx->a)->Flex1())
                   ->Child(gChangeTheme.ddFollow.Build(cx, StrL("theme-follow"), kThemeComboDx)));
    // Enter runs Change, so it is drawn as the default button
    col->Child(DlgFooter(cx, Tr("Change"), gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnOk),
                         gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnCancel)));
    return col->Bg(ToGpui(ThemeWindowControlBackgroundColor()));
}

static gp::El* ChangeThemeToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gChangeTheme.visible || !gChangeTheme.tw) {
        return nullptr;
    }
    if (!gChangeThemeView.IsValid()) {
        gChangeThemeView = gp::EntityNewState<ChangeThemeView>(cx->app);
    }
    return gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->Child(ChangeThemeColumn(cx));
}

// orig's closeOnEsc (which cancels), Enter for the default button, and the
// list's keys
static bool ChangeThemeToolOnKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    if (!gChangeTheme.visible) {
        return false;
    }
    if (ev->vk == VK_ESCAPE) {
        CloseChangeThemeDialog();
        return true;
    }
    if (ev->vk == VK_RETURN) {
        ChangeThemeOk();
        return true;
    }
    if (gChangeTheme.documentColorsFollowThemeOnly || ev->ctrl || ev->alt) {
        return false;
    }
    int n = ThemeGetCount() + 1;
    int page = std::max(std::min(n, kThemeListRows) - 1, 1);
    int sel = std::max(gChangeTheme.sel, 0);
    switch (ev->vk) {
        case VK_UP:
            return ThemeMoveSelection(-1);
        case VK_DOWN:
            return ThemeMoveSelection(1);
        case VK_PRIOR:
            ThemeSelect(std::max(sel - page, 0));
            return true;
        case VK_NEXT:
            ThemeSelect(std::min(sel + page, n - 1));
            return true;
        case VK_HOME:
            ThemeSelect(0);
            return true;
        case VK_END:
            ThemeSelect(n - 1);
            return true;
    }
    return false;
}

// orig's window has no owner: it stays when the main window it was opened
// from closes
static void ChangeThemeToolOnOwnerClosed(MainWindow* newOwner) {
    gChangeTheme.win = newOwner;
}

// the caption's close box cancels, as orig's OnClose
static void ChangeThemeToolOnClosed(MainWindow*) {
    gChangeTheme.tw = nullptr;
    CloseChangeThemeDialog();
}

// orig's PositionDialog: beside the main window, so the theme being previewed
// stays visible: on whichever side has room (the roomier one if both do), at
// the right edge of the work area if neither does; vertically centered on it
static void ChangeThemeOpenToolWindow(MainWindow* win) {
    if (gChangeTheme.tw || !ToolWindowsAvailable()) {
        return;
    }
    // orig: WS_POPUPWINDOW | WS_CAPTION, no owner
    ToolWindowDesc desc;
    desc.name = "changetheme";
    desc.title = ChangeThemeToolTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.owner = ToolWinOwner::TopLevel;
    desc.build = ChangeThemeToolBuild;
    desc.onKey = ChangeThemeToolOnKey;
    desc.onClosed = ChangeThemeToolOnClosed;
    desc.onOwnerClosed = ChangeThemeToolOnOwnerClosed;
    // the drop-down's line and the footer, the list over them
    int n = ThemeGetCount() + 1;
    float clientDy = kToolDlgLineDy + 1 + 2 * kToolDlgFootPadDy + kDlgBtnDy;
    if (!gChangeTheme.documentColorsFollowThemeOnly) {
        clientDy += (float)std::min(n, kThemeListRows) * kDlgListRowDy + 2 * kToolDlgFilesPadDy + 1;
    }
    Size size = ToolWindowOuterSize(desc, win, Size(kThemeDlgDx, (int)(clientDy + 0.5f)));

    Rect rRelative = AppShellWindowScreenRect(win);
    Rect work = AppShellWorkArea(win);
    int gap = MulDiv(8, std::max(AppShellWindowDpi(win), 96), 96);
    int spaceLeft = rRelative.x - work.x;
    int spaceRight = work.Right() - rRelative.Right();
    bool fitsLeft = spaceLeft >= size.dx + gap;
    bool fitsRight = spaceRight >= size.dx + gap;
    int x;
    if (fitsRight && (!fitsLeft || spaceRight >= spaceLeft)) {
        x = rRelative.Right() + gap;
    } else if (fitsLeft) {
        x = rRelative.x - gap - size.dx;
    } else {
        x = work.Right() - size.dx;
    }
    // vertically centered on the main window
    int y = rRelative.y + ((rRelative.dy - size.dy) / 2);
    Rect r = AppShellShiftToWorkArea({x, y, size.dx, size.dy}, win, true);
    gChangeTheme.tw = ToolWindowOpen(desc, win, r);
}

void ShowThemeDialog(MainWindow* win, bool documentColorsFollowThemeOnly) {
    if (!HasPermission(Perm::SavePreferences) || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (gChangeTheme.visible) {
        // orig: HwndSetFocus() on the one that is open
        ToolWindowActivate(gChangeTheme.tw);
        return;
    }
    gChangeTheme.win = win;
    gChangeTheme.documentColorsFollowThemeOnly = documentColorsFollowThemeOnly;
    gChangeTheme.startThemePref = str::Dup(gSettings->theme);
    gChangeTheme.startDocumentColorsFollowTheme = GetDocumentColorsFollowTheme();
    gChangeTheme.sel = kFollowWindowsThemeListIndex;
    if (!str::EqI(gSettings->theme, StrL("System"))) {
        gChangeTheme.sel = ThemeGetCurrentIndex() + 1;
    }
    gChangeTheme.scrollY = 0;
    ScrollSelIntoView();
    StrVec modes;
    for (int i = 0; i < 3; i++) {
        modes.Append(SeqStrByIndex(gDocumentColorsFollowThemeNames, i));
    }
    gChangeTheme.ddFollow.Init(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gChangeTheme.ddFollow.SetItems(modes, FollowThemeToIndex(gChangeTheme.startDocumentColorsFollowTheme));
    gChangeTheme.previewMs = -1;
    gChangeTheme.visible = true;
    ChangeThemeOpenToolWindow(win);
    ScrollSelIntoView();
    AppShellInvalidate(win);
}

gp::El* ChangeThemeDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gChangeTheme.visible || gChangeTheme.tw || gChangeTheme.win != win) {
        return nullptr;
    }
    if (!gChangeThemeView.IsValid()) {
        gChangeThemeView = gp::EntityNewState<ChangeThemeView>(cx->app);
    }
    DlgSetDefault(cx, gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnOk));
    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(ChangeThemeToolTitle()))
        ->Body(ChangeThemeColumn(cx))
        ->Footer(gp::Div(cx->a)->H(0))
        ->W(kThemeDlgDx)
        ->OnClose(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnCancel))
        ->IntoEl(gp::WindowSize(cx->win));
}
