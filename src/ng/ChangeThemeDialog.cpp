/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's ChangeThemeDialog.cpp is a WS_POPUPWINDOW placed beside the main
// window so the page stays visible while the theme is previewed. gpui cannot
// place a window, so this is a gpui Dialog with the same rows: the theme list
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
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"

#include "SumatraLog.h"

static constexpr int kFollowWindowsThemeListIndex = 0;
static constexpr int kThemePreviewDebounceMs = 300;
static constexpr float kThemeRowDy = 22;
static constexpr float kThemeListMinDy = 180;
static constexpr float kThemeListMaxDy = 320;
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

// orig's sizes for the window: 280 wide, 4 / 8 around, a 16 row list of 19
// high rows in the 12 px app font, the label 8 under it, the drop-down 4
// under that, 25 high buttons with 4 above and below
constexpr int kThemeWinDx = 280;
constexpr float kThemeWinPadX = 8;
constexpr float kThemeWinPadY = 4;
constexpr float kThemeWinRowDy = 19;
constexpr int kThemeWinListRows = 16;
constexpr float kThemeWinFontPx = 12;
constexpr float kThemeWinLabelDy = 15;
constexpr float kThemeWinLabelGap = 8;
constexpr float kThemeWinDropDownDy = 23;
constexpr float kThemeWinBtnDy = 25;
constexpr float kThemeWinBtnMinDx = 70;
constexpr float kThemeWinBtnPadDx = 12;
constexpr float kThemeWinGap = 4;

static float ThemeRowDy() {
    return gChangeTheme.tw ? kThemeWinRowDy : kThemeRowDy;
}

// the height of the list's view
static float ThemeListViewDy() {
    int n = ThemeGetCount() + 1;
    if (gChangeTheme.tw) {
        return (float)std::min(n, kThemeWinListRows) * kThemeWinRowDy;
    }
    return DialogListViewDy(n, kThemeRowDy, kThemeListMinDy, kThemeListMaxDy);
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
        DialogScrollToRow(gChangeTheme.scrollY, gChangeTheme.sel, n, ThemeRowDy(), ThemeListViewDy());
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

static gp::El* ChangeThemeToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gChangeTheme.visible || !gChangeTheme.tw) {
        return nullptr;
    }
    if (!gChangeThemeView.IsValid()) {
        gChangeThemeView = gp::EntityNewState<ChangeThemeView>(cx->app);
    }
    // orig's OnDocumentColorsFollowThemeChanged
    if (gChangeTheme.ddFollow.PollChanged(cx->app)) {
        SetDocumentColorsFollowTheme(FollowThemeFromIndex(gChangeTheme.ddFollow.sel));
        PreviewDocumentColors();
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float fontScale = ToolWindowSetUiFontPx(cx, kThemeWinFontPx);
    float font = kThemeWinFontPx * fontScale;
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->PadX(kThemeWinPadX)->PadY(kThemeWinPadY);

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
            Str name = (i == kFollowWindowsThemeListIndex) ? Tr("Follow Windows") : ThemeGetNameAt(i - 1);
            gp::El* row = gp::Div(cx->a)
                              ->FlexRow()
                              ->W(gp::kFill)
                              ->H(kThemeWinRowDy)
                              ->Shrink0()
                              ->ItemsCenter()
                              ->PadX(4)
                              ->PathClick(GpuiDup(cx->a, fmt("theme-row-%d", i)))
                              ->OnClick(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnRowClick, (intptr_t)i));
            if (i == gChangeTheme.sel) {
                row->Bg(ToGpui(AccentColor(ThemeWindowControlBackgroundColor(), 30)));
            }
            row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, name))->Font(font)->Fg(th.foreground));
            list->Child(row);
        }
        col->Child(list);
        col->Child(
            gp::Div(cx->a)
                ->FlexRow()
                ->ItemsCenter()
                ->H(kThemeWinLabelGap + kThemeWinLabelDy)
                ->PadT(kThemeWinLabelGap)
                ->Shrink0()
                ->Child(gp::TextEl(cx->a, ToGpui(Tr("Document colors follow theme")))->Font(font)->Fg(th.foreground)));
    }
    col->Child(
        gp::Div(cx->a)
            ->FlexRow()
            ->ItemsCenter()
            ->W(gp::kFill)
            ->H(kThemeWinGap + kThemeWinDropDownDy)
            ->PadT(kThemeWinGap)
            ->Shrink0()
            ->Child(gChangeTheme.ddFollow.Build(cx, StrL("theme-follow"), (float)kThemeWinDx - 2 * kThemeWinPadX)));

    auto button = [&](gp::Str id, Str label, gp::Listener onClick, bool isDefault) {
        gpc::Button* b = gpc::Button::New(cx, id)->Label(ToGpui(label))->OnClick(onClick);
        if (isDefault) {
            b->Primary();
        }
        return b->IntoEl()->H(kThemeWinBtnDy)->MinW(kThemeWinBtnMinDx)->PadX(kThemeWinBtnPadDx)->Shrink0();
    };
    gp::El* buttons = gp::Div(cx->a)
                          ->FlexRow()
                          ->JustifyEnd()
                          ->ItemsCenter()
                          ->W(gp::kFill)
                          ->H(kThemeWinBtnDy + 2 * kThemeWinGap)
                          ->Gap(6)
                          ->Shrink0();
    buttons->Child(
        button(GStrL("theme-cancel"), Tr("Cancel"), gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnCancel), false));
    // Enter runs this one, so it is drawn as the default button
    buttons->Child(
        button(GStrL("theme-change"), Tr("Change"), gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnOk), true));
    col->Child(buttons);
    return col;
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
    int page = std::max(std::min(n, kThemeWinListRows) - 1, 1);
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
#if OS_WIN
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
    // the row height and the list's view depend on tw being set
    int n = ThemeGetCount() + 1;
    float listDy = (float)std::min(n, kThemeWinListRows) * kThemeWinRowDy;
    float clientDy = 2 * kThemeWinPadY + kThemeWinGap + kThemeWinDropDownDy + 2 * kThemeWinGap + kThemeWinBtnDy;
    if (!gChangeTheme.documentColorsFollowThemeOnly) {
        clientDy += listDy + kThemeWinLabelGap + kThemeWinLabelDy;
    }
    Size size = ToolWindowOuterSize(desc, win, Size(kThemeWinDx, (int)(clientDy + 0.5f)));

    HWND hwndRelative = AppShellNativeHwnd(win);
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect work = GetWorkAreaRect(rRelative, hwndRelative);
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
    Rect r = ShiftRectToWorkArea({x, y, size.dx, size.dy}, hwndRelative, true);
    gChangeTheme.tw = ToolWindowOpen(desc, win, r);
#endif
}

static void ShowThemeDialog(MainWindow* win, bool documentColorsFollowThemeOnly) {
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

void ShowChangeThemeDialog(MainWindow* win) {
    ShowThemeDialog(win, false);
}

void ShowSetDocumentColorsFollowThemeDialog(MainWindow* win) {
    ShowThemeDialog(win, true);
}

gp::El* ChangeThemeDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gChangeTheme.visible || gChangeTheme.tw || gChangeTheme.win != win) {
        return nullptr;
    }
    if (!gChangeThemeView.IsValid()) {
        gChangeThemeView = gp::EntityNewState<ChangeThemeView>(cx->app);
    }
    // orig's OnDocumentColorsFollowThemeChanged
    if (gChangeTheme.ddFollow.PollChanged(cx->app)) {
        SetDocumentColorsFollowTheme(FollowThemeFromIndex(gChangeTheme.ddFollow.sel));
        PreviewDocumentColors();
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);

    if (!gChangeTheme.documentColorsFollowThemeOnly) {
        gp::El* list = gp::Div(cx->a)
                           ->Id(GStrL("theme-list"))
                           ->FlexCol()
                           ->W(gp::kFill)
                           ->MinH(kThemeListMinDy)
                           ->MaxH(kThemeListMaxDy)
                           ->ScrollY(gChangeTheme.scrollY)
                           ->ScrollFromPath()
                           ->OnScroll(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnScroll))
                           ->Border(1, th.border);
        int nThemes = ThemeGetCount();
        for (int i = 0; i <= nThemes; i++) {
            // orig: a mode, not another color theme, so its name is distinct
            // from the persisted Theme = System value
            Str name = (i == kFollowWindowsThemeListIndex) ? Tr("Follow Windows") : ThemeGetNameAt(i - 1);
            gp::El* row = gp::Div(cx->a)
                              ->FlexRow()
                              ->W(gp::kFill)
                              ->H(kThemeRowDy)
                              ->Shrink0()
                              ->ItemsCenter()
                              ->PadX(6)
                              ->PathClick(GpuiDup(cx->a, fmt("theme-row-%d", i)))
                              ->OnClick(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnRowClick, (intptr_t)i));
            if (i == gChangeTheme.sel) {
                row->Bg(th.selection);
            }
            row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, name))->Font(13)->Fg(th.foreground));
            list->Child(row);
        }
        body->Child(list);
        body->Child(gp::TextEl(cx->a, ToGpui(Tr("Document colors follow theme")))->Font(13)->Fg(th.foreground));
    }
    body->Child(gChangeTheme.ddFollow.Build(cx, StrL("theme-follow"), gp::kFill));

    Str title =
        gChangeTheme.documentColorsFollowThemeOnly ? Tr("Make Document Colors Follow Theme") : Tr("Change Theme");
    DlgSetDefault(cx, gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnOk));
    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(title))
        ->Body(body)
        ->W(360)
        ->OkText(ToGpui(Tr("Change")))
        ->CancelText(ToGpui(Tr("Cancel")))
        ->ShowCancel(true)
        ->OnOk(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnOk))
        ->OnCancel(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnCancel))
        ->OnClose(gp::ListenTo(gChangeThemeView, &ChangeThemeView::OnCancel))
        ->IntoEl(gp::WindowSize(cx->win));
}
