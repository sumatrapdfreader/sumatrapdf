/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's GoToPageDialog.cpp is a modal WS_POPUPWINDOW: the chapter line
// (only for a chaptered document), the page line, each with its "(of %d)",
// and Cancel / Go to page. Here it is the same column of gpui elements, in a
// window of its own where the platform has one, else in a card over the frame.

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
#include "shared/PdfToolsCommon.h"

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
};

constexpr float kGoToDlgDx = 340;

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

void CloseGoToPageDialog() {
    if (!gGoToPage.visible) {
        return;
    }
    gGoToPage.visible = false;
    MainWindow* win = gGoToPage.win;
    if (gGoToPage.tw) {
        DlgWindowClose(&gGoToPage.tw);
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

static Str GoToPageTitle() {
    return Tr("Go to page");
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
    DlgWindowSpec spec;
    spec.name = "gotopage";
    spec.title = GoToPageTitle;
    spec.build = GoToPageDialogBuild;
    spec.close = CloseGoToPageDialog;
    spec.clientDx = kGoToDlgDx;
    gGoToPage.tw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

// a line: the label, the field and "(of N)" muted at the end
static gp::El* GoToLine(gp::Ctx* cx, Str label, float labelDx, gp::InputState* edit, Str id, int ofCount) {
    edit->onChange = gp::ListenTo(gGoToPageView, &GoToPageView::OnInput);
    gp::El* row = DlgLine(cx, label, labelDx, id, edit, DlgField::Plain);
    Color colMuted = ThemeWindowTextDisabledColor();
    TempStr of = fmt(Tr("(of %d)").s, ofCount);
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, of))->Font(kToolDlgMetaFontPx)->Fg(ToGpui(colMuted))->Shrink0());
    return row;
}

gp::El* GoToPageDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gGoToPage.visible || gGoToPage.win != win) {
        return nullptr;
    }
    if (gGoToPage.tw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gGoToPageView.IsValid()) {
        gGoToPageView = gp::EntityNewState<GoToPageView>(cx->app);
    }
    gp::Listener onOk = gp::ListenTo(gGoToPageView, &GoToPageView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gGoToPageView, &GoToPageView::OnCancel);
    DlgSetDefault(cx, onOk);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (gGoToPage.hasChapters) {
        // the fields start in one column; the labels are in the app font's
        // family, a size up
        float scale = kToolDlgTextFontPx / kDlgWinFontPx;
        float labelDx = std::max(DlgWinTextDx(cx, Tr("&Chapter:")), DlgWinTextDx(cx, Tr("&Page:"))) * scale;
        col->Child(GoToLine(cx, Tr("&Chapter:"), labelDx, gGoToPage.editChapter, StrL("goto-chapter"),
                            gGoToPage.chapterCount));
        col->Child(DlgHairline(cx));
        col->Child(GoToLine(cx, Tr("&Page:"), labelDx, gGoToPage.editPage, StrL("goto-page"), gGoToPage.pageCount));
    } else {
        col->Child(GoToLine(cx, Tr("&Go to page:"), 0, gGoToPage.editPage, StrL("goto-page"), gGoToPage.pageCount));
    }
    col->Child(DlgFooter(cx, Tr("Go to page"), onOk, onCancel));
    gp::El* content = DlgToolContent(cx, col, GoToPageTitle(), kGoToDlgDx, onCancel);

    if (gGoToPage.wantFocus) {
        gp::InputState* focusTarget =
            (gGoToPage.hasChapters && gGoToPage.editChapter) ? gGoToPage.editChapter : gGoToPage.editPage;
        gp::InputFocus(focusTarget, cx->app, cx->win);
        gp::InputSelectAll(focusTarget, cx->app, cx->win);
        gGoToPage.wantFocus = cx->win->input != focusTarget;
    }
    return content;
}
