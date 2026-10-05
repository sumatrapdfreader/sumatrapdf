/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's GoToPageDialog.cpp is a modal WS_POPUPWINDOW with VirtText labels
// and real Edit controls. Where the platform can have such a window
// (gui/ToolWindow.h) it is that, in orig's sizes; elsewhere a gpui Dialog with
// the same rows: the chapter field (only for a chaptered document), the page
// field, the "(of %d)" counts and the Cancel / Go to page buttons.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "EngineBase.h"
#include "DocController.h"
#include "PagePosition.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/PlatformFont.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"

struct GoToPageDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    int pageCount = 0;
    int chapterCount = 0;
    bool hasChapters = false;
    gpui::InputState* editPage = nullptr;
    gpui::InputState* editChapter = nullptr;
    bool wantFocus = false;
    // orig's modal window, where the platform can have one; null: a dialog in
    // the frame
    ToolWindow* tw = nullptr;
    // the width of the label column in that window (dips)
    float toolLabelDx = 0;
};

static GoToPageDlg gGoToPage;

struct GoToPageView {
    static void OnOk(GoToPageView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(GoToPageView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(GoToPageView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<GoToPageView> gGoToPageView;

static gp::InputState* NewNumberInput(gp::App* app, Str text) {
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(s, ToGpui(text));
    return s;
}

static void FreeInputs() {
    delete gGoToPage.editPage;
    gGoToPage.editPage = nullptr;
    delete gGoToPage.editChapter;
    gGoToPage.editChapter = nullptr;
}

// the dialog in the frame; a window of its own is not the frame's business
bool IsGoToPageDialogVisible() {
    return gGoToPage.visible && !gGoToPage.tw;
}

static void GoToPageOpenToolWindow(MainWindow* win);

void CloseGoToPageDialog() {
    if (!gGoToPage.visible) {
        return;
    }
    gGoToPage.visible = false;
    MainWindow* win = gGoToPage.win;
    if (gGoToPage.tw) {
        ToolWindowClose(gGoToPage.tw);
        gGoToPage.tw = nullptr;
    } else if (win && win->gpuiWin && gGoToPage.editPage) {
        gp::InputBlur(gGoToPage.editPage, win->gpuiWin->app, win->gpuiWin);
        if (gGoToPage.editChapter) {
            gp::InputBlur(gGoToPage.editChapter, win->gpuiWin->app, win->gpuiWin);
        }
    }
    FreeInputs();
    AppShellInvalidate(win);
}

static Str InputTextTemp(gp::InputState* s) {
    if (!s) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(s)));
}

// orig's GoToPageWnd::OnOk
static void GoToPageOk(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded() || !win->ctrl) {
        CloseGoToPageDialog();
        return;
    }
    if (gGoToPage.hasChapters) {
        int chapter = gGoToPage.editChapter ? ParseInt(InputTextTemp(gGoToPage.editChapter)) : 1;
        int page = gGoToPage.editPage ? ParseInt(InputTextTemp(gGoToPage.editPage)) : 1;
        Location loc = win->ctrl->ClampLocation({chapter, page});
        win->ctrl->GoToLocation(loc, true);
        CloseGoToPageDialog();
        return;
    }
    TempStr pageLabel = InputTextTemp(gGoToPage.editPage);
    int newPageNo = win->ctrl->GetPageByLabel(pageLabel);
    logf("GoToPageDialog: '%s' -> page %d\n", pageLabel, newPageNo);
    if (win->ctrl->ValidPageNo(newPageNo)) {
        win->ctrl->GoToPage(newPageNo, true);
    }
    CloseGoToPageDialog();
}

void GoToPageView::OnOk(GoToPageView*, gp::Ctx* cx, const gp::ClickEvent*) {
    GoToPageOk(gGoToPage.win);
    gp::Notify(cx);
}

void GoToPageView::OnCancel(GoToPageView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseGoToPageDialog();
    gp::Notify(cx);
}

void GoToPageView::OnInput(GoToPageView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    GoToPageOk(gGoToPage.win);
    gp::Notify(cx);
}

void ShowGoToPageDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded() || !win->ctrl) {
        return;
    }
    DocController* ctrl = win->ctrl;
    if (gGoToPage.visible) {
        CloseGoToPageDialog();
    }
    FreeInputs();
    gGoToPage.win = win;
    gGoToPage.hasChapters = ShowChapterUi(ctrl);
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    if (gGoToPage.hasChapters) {
        Location cur = ctrl->CurrentLocation();
        gGoToPage.chapterCount = ctrl->ChapterCount();
        gGoToPage.pageCount = ctrl->ChapterPageCount(cur.chapter);
        gGoToPage.editChapter = NewNumberInput(app, fmt("%d", cur.chapter));
        gGoToPage.editPage = NewNumberInput(app, fmt("%d", cur.page));
    } else {
        gGoToPage.chapterCount = 0;
        gGoToPage.pageCount = ctrl->PageCount();
        gGoToPage.editPage = NewNumberInput(app, ctrl->GetPageLabeTemp(ctrl->CurrentPageNo()));
    }
    gGoToPage.visible = true;
    gGoToPage.wantFocus = true;
    GoToPageOpenToolWindow(win);
    AppShellInvalidate(win);
}

// --- a window of its own (Windows) ------------------------------------------

// orig's layout at 96 dpi: a 300 wide client area, 4 / 8 around; each field
// row is the label, the 54 x 23 edit 8 after it and "(of N)" 8 after that;
// the buttons are 25 high with 4 above and below, 8 apart, at the right
constexpr int kGoToWinDx = 300;
constexpr float kGoToPadX = 8;
constexpr float kGoToPadY = 4;
constexpr float kGoToEditDx = 54;
constexpr float kGoToEditDy = 23;
constexpr float kGoToGap = 8;
constexpr float kGoToRowGap = 4;
constexpr float kGoToBtnDy = 25;
constexpr float kGoToBtnPadDx = 11;
constexpr float kGoToFontPx = 12;

static Str GoToPageToolTitle() {
    return Tr("Go to page");
}

static gp::El* GoToPageToolRow(gp::Ctx* cx, Str label, gp::InputState* edit, Str id, int ofCount, float font) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    edit->onChange = gp::ListenTo(gGoToPageView, &GoToPageView::OnInput);
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(kGoToEditDy)->Shrink0();
    row->Child(gp::Div(cx->a)
                   ->W(gGoToPage.toolLabelDx)
                   ->Shrink0()
                   ->Child(DlgAccelText(cx, DlgAccelInput(cx, label, edit))->Font(font)->Fg(th.foreground)));
    row->Child(gp::Div(cx->a)->PadX(kGoToGap)->Shrink0()->Child(gpc::Input::New(cx, GpuiDup(cx->a, id), edit)
                                                                    ->WithSize(gp::UiSize::Small)
                                                                    ->Align(gp::component::InputAlign::Right)
                                                                    ->W(kGoToEditDx)
                                                                    ->IntoEl()));
    row->Child(
        gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("(of %d)").s, ofCount)))->Font(font)->Fg(th.foreground)->Shrink0());
    return row;
}

static gp::El* GoToPageToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gGoToPage.visible || !gGoToPage.tw) {
        return nullptr;
    }
    if (!gGoToPageView.IsValid()) {
        gGoToPageView = gp::EntityNewState<GoToPageView>(cx->app);
    }
    float font = kGoToFontPx * ToolWindowSetUiFontPx(cx, kGoToFontPx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->PadX(kGoToPadX)->PadY(kGoToPadY);
    if (gGoToPage.hasChapters) {
        col->Child(GoToPageToolRow(cx, Tr("&Chapter:"), gGoToPage.editChapter, StrL("goto-chapter"),
                                   gGoToPage.chapterCount, font));
        col->Child(gp::Div(cx->a)->H(kGoToRowGap)->Shrink0());
        col->Child(GoToPageToolRow(cx, Tr("&Page:"), gGoToPage.editPage, StrL("goto-page"), gGoToPage.pageCount, font));
    } else {
        col->Child(
            GoToPageToolRow(cx, Tr("&Go to page:"), gGoToPage.editPage, StrL("goto-page"), gGoToPage.pageCount, font));
    }

    gp::Listener onOk = gp::ListenTo(gGoToPageView, &GoToPageView::OnOk);
    DlgSetDefault(cx, onOk);
    auto button = [&](gp::Str id, Str label, gp::Listener onClick, bool isDefault) {
        gpc::Button* b = gpc::Button::New(cx, id)->Label(ToGpui(label))->OnClick(onClick);
        if (isDefault) {
            b->Primary();
        }
        return b->IntoEl()->H(kGoToBtnDy)->PadX(kGoToBtnPadDx)->Shrink0();
    };
    gp::El* buttons = gp::Div(cx->a)
                          ->FlexRow()
                          ->JustifyEnd()
                          ->ItemsCenter()
                          ->W(gp::kFill)
                          ->H(kGoToBtnDy + 2 * kGoToRowGap)
                          ->Gap(kGoToGap)
                          ->Shrink0();
    buttons->Child(
        button(GStrL("goto-cancel"), Tr("Cancel"), gp::ListenTo(gGoToPageView, &GoToPageView::OnCancel), false));
    buttons->Child(button(GStrL("goto-ok"), Tr("Go to page"), onOk, true));
    col->Child(buttons);

    if (gGoToPage.wantFocus) {
        gp::InputState* focusTarget =
            (gGoToPage.hasChapters && gGoToPage.editChapter) ? gGoToPage.editChapter : gGoToPage.editPage;
        gp::InputFocus(focusTarget, cx->app, cx->win);
        gp::InputSelectAll(focusTarget, cx->app, cx->win);
        gGoToPage.wantFocus = cx->win->input != focusTarget;
    }
    return col;
}

static bool GoToPageToolOnKey(MainWindow*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    return gGoToPage.visible && ToolWindowDialogKey(cx, ev, CloseGoToPageDialog);
}

static void GoToPageToolOnClosed(MainWindow*) {
    gGoToPage.tw = nullptr;
    CloseGoToPageDialog();
}

static void GoToPageOpenToolWindow(MainWindow* win) {
    if (gGoToPage.tw || !ToolWindowsAvailable()) {
        return;
    }
#if OS_WIN
    ToolWindowDesc desc = ToolWindowModalDesc("gotopage", GoToPageToolTitle);
    desc.build = GoToPageToolBuild;
    desc.onKey = GoToPageToolOnKey;
    desc.onClosed = GoToPageToolOnClosed;
    // the labels share a column as wide as the widest of them
    int dpi = std::max(AppShellWindowDpi(win), 96);
    PlatformFont* font = GetDefaultGuiFont();
    auto labelDx = [&](Str label) {
        TempStr noAmp = str::ReplaceTemp(label, StrL("&"), StrL(""));
        return (float)MulDiv(PlatformFontMeasureText(font, noAmp).dx, 96, dpi);
    };
    int rows = 1;
    gGoToPage.toolLabelDx = labelDx(Tr("&Go to page:"));
    if (gGoToPage.hasChapters) {
        rows = 2;
        gGoToPage.toolLabelDx = std::max(labelDx(Tr("&Chapter:")), labelDx(Tr("&Page:")));
    }
    float dy =
        2 * kGoToPadY + (float)rows * kGoToEditDy + (float)(rows - 1) * kGoToRowGap + kGoToBtnDy + 2 * kGoToRowGap;
    gGoToPage.tw = ToolWindowOpen(desc, win, ToolWindowCenteredRect(desc, win, Size(kGoToWinDx, (int)dy)));
#else
    (void)win;
#endif
}

static gp::El* FieldRow(gp::Ctx* cx, Str label, gp::InputState* edit, Str id, int ofCount) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    edit->onChange = gp::ListenTo(gGoToPageView, &GoToPageView::OnInput);
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(8);
    row->Child(DlgAccelText(cx, DlgAccelInput(cx, label, edit))->Font(13)->Fg(th.foreground));
    row->Child(gpc::Input::New(cx, GpuiDup(cx->a, id), edit)
                   ->WithSize(gp::UiSize::Small)
                   ->Align(gp::component::InputAlign::Right)
                   ->W(80)
                   ->IntoEl());
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("(of %d)").s, ofCount)))->Font(13)->Fg(th.mutedFg));
    return row;
}

gp::El* GoToPageDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gGoToPage.visible || gGoToPage.tw || gGoToPage.win != win) {
        return nullptr;
    }
    if (!gGoToPageView.IsValid()) {
        gGoToPageView = gp::EntityNewState<GoToPageView>(cx->app);
    }
    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);
    if (gGoToPage.hasChapters) {
        body->Child(FieldRow(cx, Tr("&Chapter:"), gGoToPage.editChapter, StrL("goto-chapter"), gGoToPage.chapterCount));
        body->Child(FieldRow(cx, Tr("&Page:"), gGoToPage.editPage, StrL("goto-page"), gGoToPage.pageCount));
    } else {
        body->Child(FieldRow(cx, Tr("&Go to page:"), gGoToPage.editPage, StrL("goto-page"), gGoToPage.pageCount));
    }

    DlgSetDefault(cx, gp::ListenTo(gGoToPageView, &GoToPageView::OnOk));
    gp::El* dlg = gpc::Dialog::New(cx)
                      ->Open(true)
                      ->Title(ToGpui(Tr("Go to page")))
                      ->Body(body)
                      ->W(320)
                      ->OkText(ToGpui(Tr("Go to page")))
                      ->CancelText(ToGpui(Tr("Cancel")))
                      ->ShowCancel(true)
                      ->OnOk(gp::ListenTo(gGoToPageView, &GoToPageView::OnOk))
                      ->OnCancel(gp::ListenTo(gGoToPageView, &GoToPageView::OnCancel))
                      ->OnClose(gp::ListenTo(gGoToPageView, &GoToPageView::OnCancel))
                      ->IntoEl(gp::WindowSize(cx->win));

    if (gGoToPage.wantFocus) {
        gGoToPage.wantFocus = false;
        gp::InputState* focusTarget =
            (gGoToPage.hasChapters && gGoToPage.editChapter) ? gGoToPage.editChapter : gGoToPage.editPage;
        gp::InputFocus(focusTarget, cx->app, cx->win);
        gp::InputSelectAll(focusTarget, cx->app, cx->win);
    }
    return dlg;
}
